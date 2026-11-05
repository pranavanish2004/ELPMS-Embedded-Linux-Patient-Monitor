// main.cpp  --  ELPMS Phase 3: Qt dashboard entry point
//
// Usage:  ./elpms_dashboard [path/to/patient_records.db]
//
// The database path may also be given in the ELPMS_DB environment variable.
// The default is "patient_records.db" in the current directory.
#include <QApplication>
#include <QString>

#include "mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);  // sets up the GUI event system

    // Choose the database file: 1st argument > $ELPMS_DB > default.
    QString dbPath = QString::fromLocal8Bit(qgetenv("ELPMS_DB"));
    if (app.arguments().size() > 1) dbPath = app.arguments().at(1);
    if (dbPath.isEmpty()) dbPath = "patient_records.db";

    MainWindow window(dbPath);
    window.show();

    return app.exec();  // run the event loop until the window is closed
}
