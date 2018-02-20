#include "sessionmanager.h"
#include "ui_sessionmanager.h"

SessionManager::SessionManager(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SessionManager)
{
    ui->setupUi(this);

    loadWindowParameters();

    QFile file(":/default.txt");
    file.open(QIODevice::ReadOnly);

    sessionsModel = new SessionsTreeModel(file.readAll());
    file.close();

    ui->sessionsTreeView->setModel(sessionsModel);

    setSessionsActions();

    setSignalSlots();
}

SessionManager::~SessionManager()
{
    delete ui;
}

void SessionManager::closeEvent(QCloseEvent *event)
{
    saveWindowParameters();

    event->accept();
}

void SessionManager::saveWindowParameters()
{
    settings.beginGroup("SessionsManager");

    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.setValue("sessionsTreeSplitter", ui->splitter->saveState());
    settings.setValue("sessionsTreeHeader", ui->sessionsTreeView->header()->saveState());

    settings.endGroup();
}

void SessionManager::loadWindowParameters()
{
    settings.beginGroup("SessionsManager");

    resize(settings.value("size", QSize(1000, 600)).toSize());
    move(settings.value("pos", QPoint(10, 10)).toPoint());
    ui->splitter->restoreState(settings.value("sessionsTreeSplitter").toByteArray());
    ui->sessionsTreeView->header()->restoreState(settings.value("sessionsTreeHeader").toByteArray());

    settings.endGroup();
}

void SessionManager::setSignalSlots()
{
    ui->sessionsTreeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->sessionsTreeView->header(),
            SIGNAL(customContextMenuRequested(QPoint)),
            SLOT(popupConnectionsListHeaderMenu(QPoint)));
}

void SessionManager::setSessionsActions()
{
    ui->sessionsTreeView->addAction(ui->actionRename);
    ui->sessionsTreeView->addAction(ui->actionSave);
    ui->sessionsTreeView->addAction(ui->actionSave_as);
    ui->sessionsTreeView->addAction(ui->actionDelete);
    ui->sessionsTreeView->addAction(ui->actionNew_session);
    ui->sessionsTreeView->addAction(ui->actionNew_folder);
    ui->sessionsTreeView->setContextMenuPolicy(Qt::ActionsContextMenu);

    QMenu *moreMenu = new QMenu(ui->btnMore);
    moreMenu->addAction(ui->actionPreferences);
    moreMenu->addAction(ui->actionCheck_for_updates);
    moreMenu->addAction(ui->actionImport_settings);
    moreMenu->addAction(ui->actionExport_settings_file);
    moreMenu->addAction(ui->actionCheck_for_updates);
    moreMenu->addAction(ui->actionGeneral_help);
    moreMenu->addAction(ui->actionAbout);
    ui->btnMore->setMenu(moreMenu);
}

void SessionManager::popupConnectionsListHeaderMenu(QPoint position)
{
    QHeaderView *header= ui->sessionsTreeView->header();
    QMenu *menu = new QMenu(header);
    QSignalMapper *mapper = new QSignalMapper(header);

    int columnsCount = sessionsModel->columnCount(QModelIndex());

    for (int i = 0; i <= columnsCount - 1; ++i) {
        QAction *action = new QAction(menu);
        action->setText(sessionsModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        action->setCheckable(true);
        action->setChecked(!header->isSectionHidden(i));
        action->setDisabled(action->isChecked() && header->hiddenSectionCount() == columnsCount - 1);

        connect(action, SIGNAL(triggered()), mapper, SLOT(map()));
        mapper->setMapping(action, i);

        menu->addAction(action);
    }

    connect(mapper, SIGNAL(mapped(int)), this, SLOT(showHideSessionsListColumn(int)));

    menu->popup(header->viewport()->mapToGlobal(position));
}

void SessionManager::showHideSessionsListColumn(int column)
{
    QHeaderView *header= ui->sessionsTreeView->header();
    header->setSectionHidden(column, !header->isSectionHidden(column));
}

void SessionManager::on_btnOpenConnection_clicked()
{

}

void SessionManager::on_btnCancel_clicked()
{
    close();
}
