#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QMainWindow>
#include <QSettings>
#include <QMenu>
#include <QFileDialog>
#include <QSignalMapper>
#include <QSortFilterProxyModel>
#include <QMessageBox>
#include "sessionstreemodel.h"
#include "isdirtydatawidgetmapper.h"
#include "sessiondelegate.h"

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

    void sessionSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

    void on_btnOpenConnection_clicked();
    void on_btnCancel_clicked();

    void on_actionNew_folder_triggered();
    void on_actionNew_session_triggered();

    void on_actionRename_triggered();

    void on_actionSave_triggered();

    void on_actionDelete_triggered();

private:
    Ui::SessionManager *ui;

    QSettings settings;
    SessionsTreeModel *sessionsModel;
    QSortFilterProxyModel *sortedSessionsModel;
    IsDirtyDataWidgetMapper *sessionMapper;

    void saveWindowParameters();
    void loadWindowParameters();
    void setUpElements();
    void setSignalSlots();
    void setSessionsActions();
    void setSessionsModel();
    void selectForRename(const QModelIndex &index);
    void setSessionMappings();
};

#endif // SESSIONMANAGER_H
