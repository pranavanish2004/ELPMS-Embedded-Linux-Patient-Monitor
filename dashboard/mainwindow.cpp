// mainwindow.cpp  --  ELPMS Phase 3: dashboard window implementation
#include "mainwindow.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

// The SQL we run on every refresh: newest 50 rows first. Column aliases
// ("AS ...") become the table's header captions.
static const char* kLatestVitalsSql =
     "SELECT id AS 'ID', "
    "       heart_rate AS 'Heart Rate (BPM)', "
    "       recorded_at AS 'Timestamp' "
    "FROM patient_readings "
    "ORDER BY id DESC "
    "LIMIT 50";
MainWindow::MainWindow(const QString& dbPath, QWidget* parent)
    : QMainWindow(parent),
      m_model(new QSqlQueryModel(this)),  // 'this' as parent -> auto-deleted
      m_table(new QTableView(this)),
      m_refreshButton(new QPushButton(tr("Refresh Patient Vitals"), this)),
      m_status(new QLabel(this)),
      m_dbPath(dbPath) {
    setWindowTitle(tr("Embedded Linux Patient Monitor System (ELPMS)"));
    resize(520, 640);

    // --- Build the layout: button on top, table in the middle, status below.
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addWidget(m_refreshButton);
    layout->addWidget(m_table);
    layout->addWidget(m_status);
    setCentralWidget(central);

    // --- Table appearance
    m_table->setModel(m_model);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);  // read-only
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_refreshButton->setMinimumHeight(40);

    // --- Signal/slot: clicking the button calls refreshVitals().
    connect(m_refreshButton, &QPushButton::clicked,
            this, &MainWindow::refreshVitals);

    if (openDatabase(dbPath)) {
        refreshVitals();  // show data immediately instead of an empty table
    }
}

MainWindow::~MainWindow() {
    // Release the model's hold on the connection before closing it,
    // otherwise Qt prints a "connection still in use" warning.
    m_model->clear();
    if (m_db.isOpen()) m_db.close();
}

bool MainWindow::openDatabase(const QString& dbPath) {
    // Never create an empty database by accident: the logger owns the file.
    if (!QFileInfo::exists(dbPath)) {
        m_status->setText(tr("Database not found: %1\n"
                             "Start the logger first.").arg(dbPath));
        m_refreshButton->setEnabled(false);
        return false;
    }

    // "QSQLITE" is Qt's built-in SQLite driver (package libqt5sql5-sqlite).
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        m_status->setText(tr("Cannot open database: %1")
                              .arg(m_db.lastError().text()));
        m_refreshButton->setEnabled(false);
        return false;
    }
    return true;
}

void MainWindow::refreshVitals() {
    if (!m_db.isOpen()) return;

    // Run the query and hand the result to the model; the view repaints itself.
    m_model->setQuery(QString::fromUtf8(kLatestVitalsSql), m_db);

    if (m_model->lastError().isValid()) {
        m_status->setText(tr("Query failed: %1")
                              .arg(m_model->lastError().text()));
        return;
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setStretchLastSection(true);
    updateStatus();
}

// Shows a one-line summary (row count and average) under the table.
void MainWindow::updateStatus() {
    int rows = m_model->rowCount();

    if (rows == 0) {
        m_status->setText(tr("No readings yet - is the logger running?"));
        return;
    }

    double sum = 0;

    for (int r = 0; r < rows; ++r) {
        sum += m_model->data(m_model->index(r, 1)).toDouble();
    }

    m_status->setText(tr("Showing latest %1 readings | average %2 BPM")
                          .arg(rows)
                          .arg(sum / rows, 0, 'f', 1));
}
