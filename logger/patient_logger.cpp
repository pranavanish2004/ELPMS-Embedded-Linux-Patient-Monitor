
#include <iostream>
#include <string>
#include <sqlite3.h>

int main() {
    sqlite3 *db;

    if (sqlite3_open("elpms.db", &db) != SQLITE_OK) {
        std::cerr << "Database error: " << sqlite3_errmsg(db) << '\n';
        return 1;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS patient_readings ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "heart_rate INTEGER NOT NULL,"
        "recorded_at DATETIME DEFAULT CURRENT_TIMESTAMP);";

    char *error = nullptr;

    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::cerr << "Table error: " << error << '\n';
        sqlite3_free(error);
        sqlite3_close(db);
        return 1;
    }

    const char *insert_sql =
        "INSERT INTO patient_readings (heart_rate) VALUES (?);";

    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Prepare error: " << sqlite3_errmsg(db) << '\n';
        sqlite3_close(db);
        return 1;
    }

    std::string line;

    std::cout << "ELPMS Patient Logger Started\n";

    while (std::getline(std::cin, line)) {
        try {
            int bpm = std::stoi(line);

            if (bpm < 60 || bpm > 100)
                continue;

            sqlite3_bind_int(stmt, 1, bpm);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                std::cout << "Saved heart rate: "
                          << bpm << " BPM\n";
            }

            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
        }
        catch (...) {
            continue;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::cout << "Logger stopped.\n";
    return 0;
}
