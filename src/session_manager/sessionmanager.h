#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QMainWindow>
#include <QSettings>
#include <QMenu>
#include <QFileDialog>
#include <QSignalMapper>
#include <QSortFilterProxyModel>
#include <QMessageBox>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include "sessionstreemodel.h"
#include "sessiontreeitem.h"
#include "isdirtydatawidgetmapper.h"
#include "sessiondelegate.h"
#include "sessionsproxymodel.h"

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

    QSqlDatabase connectDb(SessionTreeItem *item);

private slots:
    void popupConnectionsListHeaderMenu(QPoint position);
    void showHideSessionsListColumn(int column);
    void openFileDialog(QWidget* pathWidget);

    void sessionSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

    void on_btnOpenConnection_clicked();
    void on_btnCancel_clicked();

    void on_actionNew_folder_triggered();
    void on_actionNew_session_triggered();

    void on_actionRename_triggered();

    void on_actionSave_triggered();

    void on_actionDelete_triggered();

    void on_btnTestConnection_clicked();

Q_SIGNALS:
    void connected(SessionTreeItem *item, QString connectionName);

private:
    Ui::SessionManager *ui;

    QSettings settings;
    SessionsTreeModel *sessionsModel;
    QSortFilterProxyModel *sortedSessionsModel;
    SessionsProxyModel *sessionProxyModel;
    IsDirtyDataWidgetMapper *sessionMapper;

    void saveWindowParameters();
    void loadWindowParameters();
    void setUpElements();
    void setSignalSlots();
    void setSessionsActions();
    void setSessionsModel();
    QModelIndex mapToSource(const QModelIndex &index);
    QModelIndex mapFromSource(const QModelIndex &index);
    TreeItem* selectedItem();
    void selectForRename(const QModelIndex &index);
    void setSessionMappings();
};

#endif // SESSIONMANAGER_H
