#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "consts.h"
#include <QDebug>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(QString(APP_NAME).append(" - v").append(APP_VERSION));
    loadWindowParameters();
    makeToolbarsAndMenues();

    ui->logTextEdit->setReadOnly(true);

    connectionsModel = new ConnectionsTreeModel();
    ui->connectionsTreeView->setModel(connectionsModel);

    connect(ui->connectionsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::connectionSelected);

    sessionManager = new SessionManager(this);
    connect(sessionManager, &SessionManager::connected, this, &MainWindow::serverSession_connected);
    connect(sessionManager, &SessionManager::log, this, &MainWindow::onLog);
    //sessionManager->show();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveWindowParameters();

    event->accept();
}

void MainWindow::saveWindowParameters()
{
    settings.beginGroup("MainWindow");

    settings.setValue("size", size());
    settings.setValue("pos", pos());
    settings.setValue("splitterLog", ui->splitterLog->saveState());
    settings.setValue("splitterSessionsTree", ui->splitterSessionsTree->saveState());
    settings.setValue("splitterQueryHelper", ui->splitterQueryHelper->saveState());
    settings.setValue("splitterQueryResults", ui->splitterQueryResults->saveState());

    settings.endGroup();
}

void MainWindow::loadWindowParameters()
{
    settings.beginGroup("MainWindow");

    resize(settings.value("size", QSize(1500, 1000)).toSize());
    move(settings.value("pos", QPoint(10, 10)).toPoint());
    ui->splitterLog->restoreState(settings.value("splitterLog").toByteArray());
    ui->splitterSessionsTree->restoreState(settings.value("splitterSessionsTree").toByteArray());
    ui->splitterQueryHelper->restoreState(settings.value("splitterQueryHelper").toByteArray());
    ui->splitterQueryResults->restoreState(settings.value("splitterQueryResults").toByteArray());

    settings.endGroup();
}

void MainWindow::makeToolbarsAndMenues() {
    // Connection toolbar button
    makeConnectionButtons();

    // Refresh toolbar button
    makeRefreshButtons();

    // Run button/menu
    makeRunButtons();
}

void MainWindow::makeConnectionButtons()
{
    QMenu *connectionsMenu = new QMenu(tr("Connections"), this);
    connectionsMenu->addAction(ui->actionSession_manager);
    connectionsMenu->addAction(ui->actionNew_window);
    connectionsMenu->addSeparator();

    //SessionsFlatProxyModel *fm = new SessionsFlatProxyModel();
    //fm->setSourceModel(sessionsModel);

    //connectionsMenu->addActions(fm->getSessionActionsList());


    QToolButton *connectionButton = new QToolButton();
    connectionButton->setDefaultAction(ui->actionSession_manager);
    connectionButton->setPopupMode(QToolButton::MenuButtonPopup);
    connectionButton->setMenu(connectionsMenu);
    ui->mainToolBar->insertWidget(ui->actionDisconnect, connectionButton);
}

void MainWindow::makeRefreshButtons()
{
    QMenu *refreshMenu = new QMenu(tr("Refresh"), this);
    refreshMenu->addAction(ui->actionAuto_refresh);
    refreshMenu->addAction(ui->actionSet_refresh_interval);
    QToolButton *refreshButton = new QToolButton();
    refreshButton->setDefaultAction(ui->actionRefresh);
    refreshButton->setPopupMode(QToolButton::MenuButtonPopup);
    refreshButton->setMenu(refreshMenu);
    ui->mainToolBar->insertWidget(ui->actionUser_manager, refreshButton);

    QAction *test = new QAction("TEST");
    ui->mainToolBar->addAction(test);
    connect(test, &QAction::triggered, this, [=]() {
        ui->logTextEdit->appendPlainText("Test message");
    });
}

void MainWindow::makeRunButtons()
{
    QMenu *runMenu = new QMenu("Run", this);
    runMenu->addAction(ui->actionExecute_SQL);
    runMenu->addAction(ui->actionRun_selection);
    runMenu->addAction(ui->actionRun_current_query);
    runMenu->addSeparator();
    runMenu->addAction(ui->actionSend_queries_one_by_one);
    runMenu->addAction(ui->actionSend_batch_in_one_go);
    QActionGroup * runGroup = new QActionGroup(this);
    runGroup->addAction(ui->actionSend_queries_one_by_one);
    runGroup->addAction(ui->actionSend_batch_in_one_go);
    ui->actionSend_queries_one_by_one->setChecked(true);
    QToolButton *runButton = new QToolButton();
    runButton->setDefaultAction(ui->actionExecute_SQL);
    runButton->setPopupMode(QToolButton::MenuButtonPopup);
    runButton->setMenu(runMenu);
    ui->SQLToolBar->insertWidget(ui->actionLoad_SQL_file, runButton);
}

void MainWindow::on_actionSession_manager_triggered()
{
    sessionManager->show();
}

void MainWindow::serverSession_connected(DbConnection *connection)
{
    qWarning() << "Connected to database" << connection->connectionUuid.toString();

    QModelIndex connected = connectionsModel->addConnection(connection, ui->connectionsTreeView->selectionModel()->currentIndex());

    ui->connectionsTreeView->setCurrentIndex(connected);
    ui->connectionsTreeView->selectionModel()->select(connected, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);

    QSqlDatabase db = QSqlDatabase::database(connection->connectionUuid.toString());

    QSqlQueryModel *model = new QSqlQueryModel();
    model->setQuery("SHOW VARIABLES; SHOW GLOBAL VARIABLES;", db);
    model->setHeaderData(0, Qt::Horizontal, tr("Variable"));
    model->setHeaderData(1, Qt::Horizontal, tr("Session"));
    model->setHeaderData(2, Qt::Horizontal, tr("Global"));
    ui->tblViewVariables->setModel(model);



    QSqlQueryModel *statusModel = new QSqlQueryModel();
    statusModel->setQuery("SHOW STATUS;", db);
    ui->tblViewStatus->setModel(statusModel);

    QSqlQueryModel *processesModel = new QSqlQueryModel();
    processesModel->setQuery("SELECT `ID`, `USER`, `HOST`, `DB`, `COMMAND`, `TIME`, `STATE`, LEFT(`INFO`, 51200) AS `Info`, `TIME_MS`, `ROWS_SENT`, `ROWS_EXAMINED` FROM `information_schema`.`PROCESSLIST`;", db);
    processesModel->setHeaderData(0, Qt::Horizontal, "qwe");
    ui->tblViewProcesses->setModel(processesModel);

}

void MainWindow::onLog(QString message)
{
    ui->logTextEdit->appendPlainText(message);
}

void MainWindow::connectionSelected(const QItemSelection &selected, const QItemSelection &deselected)
{
    QModelIndex index = ui->connectionsTreeView->selectionModel()->currentIndex();
    TreeItem *item = connectionsModel->getItem(index);

    QSqlDatabase db = QSqlDatabase::database(item->name());

    QSqlQueryModel *databases = new QSqlQueryModel();
    databases->setQuery("SHOW DATABASES;", db);
    ui->tblViewDatabases->setModel(databases);
}
