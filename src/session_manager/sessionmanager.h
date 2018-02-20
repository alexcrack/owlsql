#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QMainWindow>
#include <QSettings>
#include <QMenu>
#include <QSignalMapper>
#include "sessionstreemodel.h"

namespace Ui {
class SessionManager;
}

class SessionManager : public QMainWindow
{
    Q_OBJECT

public:
    explicit SessionManager(QWidget *parent = 0);
    ~SessionManager();
    void closeEvent(QCloseEvent *event);

private slots:
    void popupConnectionsListHeaderMenu(QPoint position);
    void showHideSessionsListColumn(int column);

    void on_btnOpenConnection_clicked();

    void on_btnCancel_clicked();

private:
    Ui::SessionManager *ui;

    QSettings settings;
    SessionsTreeModel *sessionsModel;

    void saveWindowParameters();
    void loadWindowParameters();
    void setSignalSlots();
    void setSessionsActions();
};

#endif // SESSIONMANAGER_H
