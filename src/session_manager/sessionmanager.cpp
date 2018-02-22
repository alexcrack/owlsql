#include "sessionmanager.h"
#include "ui_sessionmanager.h"

SessionManager::SessionManager(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SessionManager)
{
    ui->setupUi(this);

    loadWindowParameters();
    setSessionsActions();
    setSignalSlots();
    setSessionsModel();
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

    // Set up open file buttons
    QSignalMapper *openFileMapper = new QSignalMapper(this);

    connect(ui->tbtnOpenSshKey, SIGNAL(clicked()), openFileMapper, SLOT(map()));
    openFileMapper->setMapping(ui->tbtnOpenSshKey, ui->editSshKeyPath);
    ui->editSshKeyPath->setProperty("p_fileTitle", ui->lblSshPrivateKey->text());
    ui->editSshKeyPath->setProperty("p_fileFilter", tr("PuTTY private key (*.ppk);;All files (*.*)"));

    connect(ui->tbtnPrivateKeyOpen, SIGNAL(clicked()), openFileMapper, SLOT(map()));
    openFileMapper->setMapping(ui->tbtnPrivateKeyOpen, ui->editSslPrivateKeyPath);
    ui->editSslPrivateKeyPath->setProperty("p_fileTitle", ui->lblSslPrivateKey->text());
    ui->editSslPrivateKeyPath->setProperty("p_fileFilter", tr("Privacy Enhanced Mail certificates (*.pem);;Certificates (*.crt);;All files (*.*)"));

    connect(ui->tbtnCaCertificateOpen, SIGNAL(clicked()), openFileMapper, SLOT(map()));
    openFileMapper->setMapping(ui->tbtnCaCertificateOpen, ui->editCaCertificatePath);
    ui->editCaCertificatePath->setProperty("p_fileTitle", ui->lblSslCaCert->text());
    ui->editCaCertificatePath->setProperty("p_fileFilter", tr("Privacy Enhanced Mail certificates (*.pem);;Certificates (*.crt);;All files (*.*)"));

    connect(ui->tbtnCertificateOpen, SIGNAL(clicked()), openFileMapper, SLOT(map()));
    openFileMapper->setMapping(ui->tbtnCertificateOpen, ui->editCertificatePath);
    ui->editCertificatePath->setProperty("p_fileTitle", ui->lblSslCert->text());
    ui->editCertificatePath->setProperty("p_fileFilter", tr("Privacy Enhanced Mail certificates (*.pem);;Certificates (*.crt);;All files (*.*)"));

    connect(ui->tbtnOpenStartupScript, SIGNAL(clicked()), openFileMapper, SLOT(map()));
    openFileMapper->setMapping(ui->tbtnOpenStartupScript, ui->editStartupScriptPath);
    ui->editStartupScriptPath->setProperty("p_fileTitle", ui->lblStartupScript->text());
    ui->editStartupScriptPath->setProperty("p_fileFilter", tr("SQL-files (*.sql);;All files (*.*)"));

#if (defined (_WIN32) || defined (_WIN64))
    connect(ui->tbtnOpenPlink, SIGNAL(clicked()), openFileMapper, SLOT(map()));
    openFileMapper->setMapping(ui->tbtnOpenPlink, ui->editSshPlinkPath);
    ui->editSshPlinkPath->setProperty("p_fileTitle", ui->lblSshPlinkPath->text());
    ui->editSshPlinkPath->setProperty("p_fileFilter", tr("Executables (*.exe);;All files (*.*)"));
#else
    ui->grpPlink->hide();
#endif

    connect(openFileMapper, SIGNAL(mapped(QWidget*)), this, SLOT(openFileDialog(QWidget*)));
}

void SessionManager::openFileDialog(QWidget* pathWidget)
{
    QLineEdit *pathEdit = qobject_cast<QLineEdit*>(pathWidget);

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    QString(tr("Select %1")).arg(pathEdit->property("p_fileTitle").toString()),
                                                    "",
                                                    pathEdit->property("p_fileFilter").toString());

    pathEdit->setText(fileName);
}

void SessionManager::setSessionsModel()
{
    sessionsModel = new SessionsTreeModel();

    sortedSessionsModel = new QSortFilterProxyModel();
    sortedSessionsModel->setSourceModel(sessionsModel);

    ui->sessionsTreeView->setModel(sortedSessionsModel);
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

    QWidget *spacer = new QWidget(ui->toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spacer->setVisible(true);
    ui->toolBar->insertWidget(ui->actionCheck_for_updates, spacer);
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

void SessionManager::selectForRename(const QModelIndex &index)
{
    ui->sessionsTreeView->setCurrentIndex(sortedSessionsModel->mapFromSource(index));
    ui->sessionsTreeView->selectionModel()->select(sortedSessionsModel->mapFromSource(index), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    ui->sessionsTreeView->edit(sortedSessionsModel->mapFromSource(index));
}

void SessionManager::on_actionNew_folder_triggered()
{
    QModelIndex selectedIndex = sortedSessionsModel->mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());

    QModelIndex created = sessionsModel->createFolder(QString("New Folder"), selectedIndex);

    selectForRename(created);
}

void SessionManager::on_actionNew_session_triggered()
{
    QModelIndex selectedIndex = sortedSessionsModel->mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());

    QModelIndex created = sessionsModel->createSession(QString("New Session"), selectedIndex);

    selectForRename(created);
}
