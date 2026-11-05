// logger.cpp  --  ELPMS Phase 2: the database logger daemon
//
// Every second this program:
//   1. opens the virtual sensor file (/dev/heart_rate),
//   2. reads one heart-rate value (text like "78\n") from it,
//   3. INSERTs the value plus a timestamp into an SQLite database.
//
// Build:   g++ -std=c++17 -Wall -Wextra -O2 logger.cpp -o logger -lsqlite3
// Run:     ./logger                          (foreground, Ctrl+C to stop)
//          ./logger --daemon                 (detach into the background)
//          ./logger --help                   (all options)

#include <sqlite3.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <unistd.h>

namespace {

// ---------------------------------------------------------------------------
// Settings (all overridable from the command line)
// ---------------------------------------------------------------------------
struct Config {
    std::string device   = "/dev/heart_rate";     // the sensor "file"
    std::string db_path  = "patient_records.db";  // SQLite database file
    std::string log_path = "";                    // daemon log (auto if empty)
    unsigned    interval = 1;                     // seconds between samples
    long        max_samples = 0;                  // 0 = run forever
    bool        daemonize = false;
};

// Set to 0 by the signal handler; the main loop checks it every iteration.
// volatile sig_atomic_t is the only type that is safe to share with a handler.
volatile std::sig_atomic_t g_running = 1;

void handle_signal(int) { g_running = 0; }

// Install handlers for Ctrl+C (SIGINT) and `kill` (SIGTERM). SA_RESTART is
// deliberately NOT set so that sleep() is interrupted and we exit promptly.
void install_signal_handlers() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

// ---------------------------------------------------------------------------
// Sensor access
// ---------------------------------------------------------------------------

// Opens the device, reads one reading and parses it. Returns true on success.
// The device is opened fresh each time: that is how the driver defines
// "one reading per open/read".
bool read_heart_rate(const std::string& device, int& bpm_out) {
    int fd = ::open(device.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[logger] cannot open " << device << ": "
                  << std::strerror(errno) << std::endl;
        return false;
    }

    char buf[32];
    ssize_t n;
    do {
        n = ::read(fd, buf, sizeof(buf) - 1);
    } while (n < 0 && errno == EINTR);  // retry if a signal interrupted us
    ::close(fd);

    if (n <= 0) {
        std::cerr << "[logger] read from " << device << " failed\n";
        return false;
    }
    buf[n] = '\0';

    // Parse the text, e.g. "78\n" -> 78, and reject garbage.
    char* end = nullptr;
    long value = std::strtol(buf, &end, 10);
    if (end == buf || value < 20 || value > 300) {
        std::cerr << "[logger] ignoring invalid sensor data: " << buf;
        return false;
    }
    bpm_out = static_cast<int>(value);
    return true;
}

// Current local time as "YYYY-MM-DD HH:MM:SS".
std::string timestamp_now() {
    std::time_t now = std::time(nullptr);
    std::tm tm_buf;
    localtime_r(&now, &tm_buf);  // thread-safe variant of localtime()
    char out[32];
    std::strftime(out, sizeof(out), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return out;
}

// ---------------------------------------------------------------------------
// Database
// ---------------------------------------------------------------------------

// Tiny RAII wrapper: the database is closed automatically when it goes out of
// scope, even if we leave early because of an error.
class Database {
public:
    explicit Database(const std::string& path) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            std::cerr << "[logger] cannot open database " << path << ": "
                      << sqlite3_errmsg(db_) << std::endl;
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }
    ~Database() {
        if (insert_stmt_) sqlite3_finalize(insert_stmt_);
        if (db_) sqlite3_close(db_);
    }
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool ok() const { return db_ != nullptr; }

    // Create the table (if needed) and prepare the INSERT statement once.
    bool init() {
        // WAL mode lets the Qt dashboard READ while we WRITE without locking
        // each other out. busy_timeout makes SQLite wait instead of failing.
        exec("PRAGMA journal_mode=WAL;");
        sqlite3_busy_timeout(db_, 2000);

        const char* create_sql =
            "CREATE TABLE IF NOT EXISTS readings ("
            "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  bpm         INTEGER NOT NULL,"
            "  recorded_at TEXT    NOT NULL"
            ");";
        if (!exec(create_sql)) return false;

        // A prepared statement is compiled once and re-used: faster and,
        // more importantly, immune to SQL injection because values are bound.
        const char* insert_sql =
            "INSERT INTO readings (bpm, recorded_at) VALUES (?, ?);";
        if (sqlite3_prepare_v2(db_, insert_sql, -1, &insert_stmt_, nullptr) !=
            SQLITE_OK) {
            std::cerr << "[logger] prepare failed: " << sqlite3_errmsg(db_)
                      << std::endl;
            return false;
        }
        return true;
    }

    // Run the prepared INSERT with the given values.
    bool insert_reading(int bpm, const std::string& ts) {
        sqlite3_reset(insert_stmt_);
        sqlite3_clear_bindings(insert_stmt_);
        sqlite3_bind_int(insert_stmt_, 1, bpm);
        sqlite3_bind_text(insert_stmt_, 2, ts.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(insert_stmt_) != SQLITE_DONE) {
            std::cerr << "[logger] insert failed: " << sqlite3_errmsg(db_)
                      << std::endl;
            return false;
        }
        return true;
    }

private:
    bool exec(const char* sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::cerr << "[logger] SQL error: " << (err ? err : "?")
                      << std::endl;
            sqlite3_free(err);
            return false;
        }
        return true;
    }

    sqlite3*      db_          = nullptr;
    sqlite3_stmt* insert_stmt_ = nullptr;
};

// ---------------------------------------------------------------------------
// Daemonizing (classic Unix recipe)
// ---------------------------------------------------------------------------
bool daemonize(const std::string& log_path) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid > 0) {  // parent: report the child's PID and leave
        std::cout << "[logger] running in background, pid " << pid
                  << ", log: " << log_path << std::endl;
        std::_Exit(0);
    }

    if (setsid() < 0) return false;  // new session: detach from the terminal

    // We never touch the terminal again: stdin from /dev/null, stdout/stderr
    // into the log file.
    int null_fd = ::open("/dev/null", O_RDONLY);
    int log_fd = ::open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (null_fd < 0 || log_fd < 0) return false;
    dup2(null_fd, STDIN_FILENO);
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    ::close(null_fd);
    ::close(log_fd);
    return true;
}

void print_usage(const char* prog) {
    std::cout
        << "Usage: " << prog << " [options]\n"
        << "  --device PATH     sensor file to read   (default /dev/heart_rate)\n"
        << "  --db PATH         SQLite database file  (default patient_records.db)\n"
        << "  --interval SEC    seconds between reads (default 1)\n"
        << "  --count N         stop after N samples  (default: run forever)\n"
        << "  --daemon          run in the background\n"
        << "  --log PATH        log file when daemonized (default <db>.log)\n"
        << "  --help            show this text\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    Config cfg;

    // Minimal hand-written argument parser (no external dependencies).
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--device")        cfg.device = next("--device");
        else if (arg == "--db")       cfg.db_path = next("--db");
        else if (arg == "--log")      cfg.log_path = next("--log");
        else if (arg == "--interval") cfg.interval = std::strtoul(next("--interval"), nullptr, 10);
        else if (arg == "--count")    cfg.max_samples = std::strtol(next("--count"), nullptr, 10);
        else if (arg == "--daemon")   cfg.daemonize = true;
        else if (arg == "--help" || arg == "-h") { print_usage(argv[0]); return 0; }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 2;
        }
    }
    if (cfg.interval == 0) cfg.interval = 1;

    // A daemon changes behaviour around the working directory, so turn the
    // database path into an absolute path BEFORE doing anything else.
    cfg.db_path = std::filesystem::absolute(cfg.db_path).string();
    if (cfg.log_path.empty()) cfg.log_path = cfg.db_path + ".log";

    if (cfg.daemonize && !daemonize(cfg.log_path)) {
        std::cerr << "[logger] failed to daemonize\n";
        return 1;
    }

    install_signal_handlers();

    Database db(cfg.db_path);
    if (!db.ok() || !db.init()) return 1;

    std::cout << "[logger] started: device=" << cfg.device
              << " db=" << cfg.db_path << " interval=" << cfg.interval
              << "s" << std::endl;  // endl flushes, so the log file stays current

    long saved = 0;
    while (g_running) {
        int bpm = 0;
        if (read_heart_rate(cfg.device, bpm)) {
            std::string ts = timestamp_now();
            if (db.insert_reading(bpm, ts)) {
                ++saved;
                std::cout << "[logger] " << ts << "  " << bpm
                          << " BPM  (saved #" << saved << ")" << std::endl;
            }
        }
        if (cfg.max_samples > 0 && saved >= cfg.max_samples) break;
        sleep(cfg.interval);  // returns early if a signal arrives
    }

    std::cout << "[logger] stopped after " << saved << " samples" << std::endl;
    return 0;
}
