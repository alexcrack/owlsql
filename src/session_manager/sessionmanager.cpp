#include "sessionmanager.h"
#include "ui_sessionmanager.h"
#include "sessiontreeitem.h"

#include <QDebug>


SessionManager::SessionManager(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SessionManager)
{
    ui->setupUi(this);

    loadWindowParameters();
    setSessionsActions();
    setSignalSlots();
    setSessionsModel();
    setSessionMappings();
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

void SessionManager::setUpElements()
{

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

void SessionManager::setSessionMappings()
{
    sessionMapper = new IsDirtyDataWidgetMapper(this);
    sessionMapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    sessionMapper->setModel(sessionsModel);
    sessionMapper->setItemDelegate(new SessionDelegate());

    // Settings tab
    //sessionMapper->addMapping(ui->cmbNetworkType, netType, "currentIndex");
    sessionMapper->addMapping(ui->editHostname, SessionTreeItem::HostName);
    //sessionMapper->addMapping(ui->chkCredentials, SessionTreeItem::);
    sessionMapper->addMapping(ui->editSessionComment, SessionTreeItem::SessionComment);
#if (defined (_WIN32) || defined (_WIN64))
    sessionMapper->addMapping(ui->chkWinAuth, windowsAuth);
#else
    ui->chkWinAuth->hide();
#endif
    sessionMapper->addMapping(ui->editUsername, SessionTreeItem::UserName);
    sessionMapper->addMapping(ui->editPassword, SessionTreeItem::Password);
    sessionMapper->addMapping(ui->spinPort, SessionTreeItem::Port);
    //sessionMapper->addMapping(ui->chkCompressed, compressed);
    //sessionMapper->addMapping(ui->cmbDatabase, allDatabaseStr, "currentText");
    //sessionMapper->addMapping(ui->editSessionComment, sessionComment);

    // SSH settings tab
    //sessionMapper->addMapping(ui->editSshHost, sshHost);
    //sessionMapper->addMapping(ui->spinSshPort, sshPort);
    //sessionMapper->addMapping(ui->editSshUser, sshUser);
    //sessionMapper->addMapping(ui->editSshPassword, sshPassword);
    //sessionMapper->addMapping(ui->editSshKeyPath, sshPrivateKey);
    //sessionMapper->addMapping(ui->spinSshLocalPort, sshLocalPort);
#if (defined (_WIN32) || defined (_WIN64))
    sessionMapper->addMapping(ui->editSshPlinkPath, sshPlinkExe);
    sessionMapper->addMapping(ui->spinPlinkTimeout, sshPlinkTimeout);
#endif

    // Advances settings tab
    //sessionMapper->addMapping(ui->grpBoxSsl, wantSsl);
    //sessionMapper->addMapping(ui->editSslPrivateKeyPath, sslPrivateKey);
    //sessionMapper->addMapping(ui->editCaCertificatePath, sslCaCertificate);
    //sessionMapper->addMapping(ui->editCertificatePath, sslCertificate);
    //sessionMapper->addMapping(ui->editSslCipher, sslCipher);

    //sessionMapper->addMapping(ui->editStartupScriptPath, startupScriptFilename);
    //sessionMapper->addMapping(ui->spinQueryTimeout, queryTimeOut);
    //sessionMapper->addMapping(ui->spinPingTimeout, pingTimeOut);
    //sessionMapper->addMapping(ui->chkClientTimeZone, clientTimeZone);
    //sessionMapper->addMapping(ui->chkFullTableStatus, fullTableStatus);

    // Statistics tab
    //sessionMapper->addMapping(ui->lblCreated, sessionCreated);
    //sessionMapper->addMapping(ui->lblLastConnect, lastConnect);
    //sessionMapper->addMapping(ui->lblSuccessConnects, connectCount, "text");
    //sessionMapper->addMapping(ui->lblUnsuccessConnects, refusedCount, "text");

//    for (int i = netType; i < SL_NO_MORE_COLUMNS; i++) {
//        ui->connectionsList->hideColumn(i);
//    }
    connect(ui->sessionsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SessionManager::sessionSelectionChanged);

    connect(sessionMapper, &IsDirtyDataWidgetMapper::dirtyStateChanged, this, [=](const bool isDirty) {
        qDebug() << "Is Dirty" << isDirty;

        ui->actionSave->setEnabled(isDirty);

        sessionsModel->setDirty(sortedSessionsModel->mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex()), isDirty);
    });
}

void SessionManager::sessionSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    // Remove tabs and add only needed for selected item
    while (ui->tabWidget->count() > 0)
        ui->tabWidget->removeTab(0);

    QModelIndex index = sortedSessionsModel->mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());
    sessionMapper->setRootIndex(index.parent());
    sessionMapper->setCurrentModelIndex(index);

    TreeItem *item = sessionsModel->getItem(index);

    if (index.isValid() && item->canEdit()) {
        ui->btnOpenConnection->setEnabled(true);
        ui->btnTestConnection->setEnabled(true);

        ui->tabWidget->addTab(ui->tabSettings, QIcon(":/icons/wrench.png"), QString(tr("Settings")));

        //        if (item->server().netType == ntMySQL_SSHtunnel)
        //            ui->tabWidget->addTab(ui->tabSshTunnel, QIcon(":/icons/lock_blue.png"), QString(tr("SSH Tunnel")));

        ui->tabWidget->addTab(ui->tabAdvancedOptions, QIcon(":/icons/wrench_orange.png"), QString(tr("Advanced")));
        ui->tabWidget->addTab(ui->tabStatistics, QIcon(":/icons/chart_bar.png"), QString(tr("Statistics")));
    } else {
        ui->btnOpenConnection->setEnabled(false);
        ui->btnTestConnection->setEnabled(false);

        ui->tabWidget->addTab(ui->tabStart, QIcon(":/icons/star.png"), QString(tr("Start")));
    }

    // Actions enabled
//    if (item->isServer()) {
//        ui->actionSave_as->setEnabled(true);
//    } else {
//        ui->actionSave->setEnabled(false);
//        ui->actionSave_as->setEnabled(false);
//    }
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

void SessionManager::on_actionRename_triggered()
{
    QModelIndex selectedIndex = sortedSessionsModel->mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());

    ui->sessionsTreeView->edit(sortedSessionsModel->mapFromSource(selectedIndex));
}

void SessionManager::on_actionSave_triggered()
{
    sessionsModel->saveModelData();
}

void SessionManager::on_actionDelete_triggered()
{
    QModelIndex index = sortedSessionsModel->mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());
    //TreeItem *item = sessionsModel->getItem(index);

    QMessageBox confirm;
    confirm.setWindowTitle(QString(tr("Confirm")));
    confirm.setText(QString(tr("Are really want to delete session \"%1\"?").arg("")));
    confirm.setIcon(QMessageBox::Question);
    confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirm.setDefaultButton(QMessageBox::Yes);

    if (confirm.exec() == QMessageBox::Yes) {
        // Delete session
        sessionsModel->deleteSession(index);
    } else {
        confirm.close();
    };
}
