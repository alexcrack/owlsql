#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlQueryModel>
#include "session_manager/sessionmanager.h"
#include "connectionstreemodel.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void closeEvent(QCloseEvent *event);

public slots:
    void on_serverSession_connected(SessionTreeItem *item, QString connectionName);

private slots:
    void on_actionSession_manager_triggered();

    void connectionSelected(const QItemSelection &selected, const QItemSelection &deselected);

private:
    Ui::MainWindow *ui;

    QSettings settings;
    SessionManager sessionManager;

    ConnectionsTreeModel *connectionsModel;

    void saveWindowParameters();
    void loadWindowParameters();

    void makeToolbarsAndMenues();
    void makeConnectionButtons();
    void makeRefreshButtons();
    void makeRunButtons();


};

#endif // MAINWINDOW_H
