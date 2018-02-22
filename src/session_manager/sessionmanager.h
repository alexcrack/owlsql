#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QMainWindow>
#include <QSettings>
#include <QMenu>
#include <QFileDialog>
#include <QSignalMapper>
#include <QSortFilterProxyModel>
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
    void openFileDialog(QWidget* pathWidget);
    void on_btnOpenConnection_clicked();

    void on_btnCancel_clicked();

    void on_actionNew_folder_triggered();

    void on_actionNew_session_triggered();

private:
    Ui::SessionManager *ui;

    QSettings settings;
    SessionsTreeModel *sessionsModel;
    QSortFilterProxyModel *sortedSessionsModel;

    void saveWindowParameters();
    void loadWindowParameters();
    void setSignalSlots();
    void setSessionsActions();
    void setSessionsModel();
    void selectForRename(const QModelIndex &index);
};

#endif // SESSIONMANAGER_H
