// mainwindow.h  --  ELPMS Phase 3: dashboard window declaration
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>

class QPushButton;
class QSqlQueryModel;
class QTableView;
class QLabel;

// The single window of the app: a "Refresh Patient Vitals" button, a table of
// the 50 most recent heart-rate readings and a status line underneath.
class MainWindow : public QMainWindow {
    Q_OBJECT  // required for signals/slots (processed by Qt's moc tool)

public:
    // dbPath: path to the SQLite file written by the logger daemon.
    explicit MainWindow(const QString& dbPath, QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    // Re-runs the SQL query and updates the table. Connected to the button.
    void refreshVitals();

private:
    bool openDatabase(const QString& dbPath);
    void updateStatus();

    QSqlDatabase    m_db;
    QSqlQueryModel* m_model;      // holds the query result for the view
    QTableView*     m_table;
    QPushButton*    m_refreshButton;
    QLabel*         m_status;
    QString         m_dbPath;
};

#endif  // MAINWINDOW_H
