#include "sessionmanager.h"
#include "ui_sessionmanager.h"

#include <QDebug>

SessionManager::SessionManager(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SessionManager)
{
    ui->setupUi(this);

    setUpElements();
    setSessionsActions();
    setSignalSlots();
    setSessionsModel();
    setSessionMappings();
    loadWindowParameters();
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

    for (int i = SessionTreeItem::Driver; i <= SessionTreeItem::FullTableStatus; i++) {
        ui->sessionsTreeView->hideColumn(i);
    }
}

void SessionManager::setUpElements()
{
    QWidget *spacer = new QWidget(ui->toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    spacer->setVisible(true);
    ui->toolBar->insertWidget(ui->actionCheck_for_updates, spacer);

    // Remove tabs and add only needed for selected item
    while (ui->tabWidget->count() > 0)
        ui->tabWidget->removeTab(0);

    ui->tabWidget->addTab(ui->tabStart, QIcon(":/icons/star.png"), QString(tr("Start")));
}

void SessionManager::setSignalSlots()
{
    ui->sessionsTreeView->header()->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->sessionsTreeView->header(), &QHeaderView::customContextMenuRequested, this, &SessionManager::popupConnectionsListHeaderMenu);

    connect(ui->tbtnOpenSshKey, &QToolButton::clicked, this, [this]() {
        openFileDialog(ui->editSshKeyPath);
    });
    ui->editSshKeyPath->setProperty("p_fileTitle", ui->lblSshPrivateKey->text());
    ui->editSshKeyPath->setProperty("p_fileFilter", tr("PuTTY private key (*.ppk);;All files (*.*)"));

    connect(ui->tbtnPrivateKeyOpen, &QToolButton::clicked, this, [this]() {
        openFileDialog(ui->editSslPrivateKeyPath);
    });
    ui->editSslPrivateKeyPath->setProperty("p_fileTitle", ui->lblSslPrivateKey->text());
    ui->editSslPrivateKeyPath->setProperty("p_fileFilter", tr("Privacy Enhanced Mail certificates (*.pem);;Certificates (*.crt);;All files (*.*)"));

    connect(ui->tbtnCaCertificateOpen, &QToolButton::clicked, this, [this]() {
        openFileDialog(ui->editCaCertificatePath);
    });
    ui->editCaCertificatePath->setProperty("p_fileTitle", ui->lblSslCaCert->text());
    ui->editCaCertificatePath->setProperty("p_fileFilter", tr("Privacy Enhanced Mail certificates (*.pem);;Certificates (*.crt);;All files (*.*)"));

    connect(ui->tbtnCertificateOpen, &QToolButton::clicked, this, [this]() {
        openFileDialog(ui->editCertificatePath);
    });
    ui->editCertificatePath->setProperty("p_fileTitle", ui->lblSslCert->text());
    ui->editCertificatePath->setProperty("p_fileFilter", tr("Privacy Enhanced Mail certificates (*.pem);;Certificates (*.crt);;All files (*.*)"));

    connect(ui->tbtnOpenStartupScript, &QToolButton::clicked, this, [this]() {
        openFileDialog(ui->editStartupScriptPath);
    });
    ui->editStartupScriptPath->setProperty("p_fileTitle", ui->lblStartupScript->text());
    ui->editStartupScriptPath->setProperty("p_fileFilter", tr("SQL-files (*.sql);;All files (*.*)"));

#if (defined (_WIN32) || defined (_WIN64))
    connect(ui->tbtnOpenPlink, &QToolButton::clicked, this, [this]() { openFileDialog(ui->editSshPlinkPath); });
    ui->editSshPlinkPath->setProperty("p_fileTitle", ui->lblSshPlinkPath->text());
    ui->editSshPlinkPath->setProperty("p_fileFilter", tr("Executables (*.exe);;All files (*.*)"));
#else
    ui->grpPlink->hide();
#endif

    //connect(ui->cmbNetworkType, &QComboBox::currentIndexChanged, this, &SessionManager::connectionTypeChanged);

    connect(ui->chkCredentials, &QCheckBox::toggled, this, &SessionManager::loginPromptChanged);
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

    sessionProxyModel = new SessionsProxyModel();
    sessionProxyModel->setSourceModel(sessionsModel);

    sortedSessionsModel = new QSortFilterProxyModel();
    sortedSessionsModel->setSourceModel(sessionProxyModel);

    ui->sessionsTreeView->setModel(sortedSessionsModel);
}

QModelIndex SessionManager::mapToSource(const QModelIndex &index)
{
    return sessionProxyModel->mapToSource(sortedSessionsModel->mapToSource(index));
}

QModelIndex SessionManager::mapFromSource(const QModelIndex &index)
{
    return sortedSessionsModel->mapFromSource(sessionProxyModel->mapFromSource(index));
}

TreeItem* SessionManager::selectedItem()
{
    QModelIndex index = ui->sessionsTreeView->selectionModel()->currentIndex();

    return sessionsModel->getItem(mapToSource(index));
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
}

void SessionManager::popupConnectionsListHeaderMenu(QPoint position)
{
    QHeaderView *header= ui->sessionsTreeView->header();
    QMenu *menu = new QMenu(header);

    for (int i = SessionTreeItem::SessionName; i <= SessionTreeItem::SessionComment; ++i) {
        QAction *action = new QAction(menu);
        action->setText(sessionsModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
        action->setCheckable(true);
        action->setChecked(!header->isSectionHidden(i));
        action->setDisabled(action->isChecked() && header->hiddenSectionCount() == SessionTreeItem::FullTableStatus - 1);

        connect(action, &QAction::triggered, this, [header, i]() {
            header->setSectionHidden(i, !header->isSectionHidden(i));
        });

        menu->addAction(action);
    }

    menu->popup(header->viewport()->mapToGlobal(position));
}

void SessionManager::setSessionMappings()
{
    sessionMapper = new IsDirtyDataWidgetMapper(this);
    sessionMapper->setModel(sessionsModel);
    sessionMapper->setItemDelegate(new SessionDelegate());
    sessionMapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);

    // Settings tab
    sessionMapper->addMapping(ui->cmbDriver, SessionTreeItem::Driver, "currentDriver");
    sessionMapper->addMapping(ui->cmbNetworkType, SessionTreeItem::NetType, "currentIndex");
    sessionMapper->addMapping(ui->editHostname, SessionTreeItem::HostName);
    sessionMapper->addMapping(ui->chkCredentials, SessionTreeItem::LoginPromt);
    sessionMapper->addMapping(ui->editSessionComment, SessionTreeItem::SessionComment);
#if (defined (_WIN32) || defined (_WIN64))
    sessionMapper->addMapping(ui->chkWinAuth, SessionTreeItem::WindowsAuth);
#else
    ui->chkWinAuth->hide();
#endif
    sessionMapper->addMapping(ui->editUsername, SessionTreeItem::UserName);
    sessionMapper->addMapping(ui->editPassword, SessionTreeItem::Password);
    sessionMapper->addMapping(ui->spinPort, SessionTreeItem::Port);
    sessionMapper->addMapping(ui->chkCompressed, SessionTreeItem::Compressed);
    sessionMapper->addMapping(ui->cmbDatabase, SessionTreeItem::AllDatabaseStr, "currentText");

    // SSH settings tab
    sessionMapper->addMapping(ui->editSshHost, SessionTreeItem::SshHost);
    sessionMapper->addMapping(ui->spinSshPort, SessionTreeItem::SshPort);
    sessionMapper->addMapping(ui->editSshUser, SessionTreeItem::SshUser);
    sessionMapper->addMapping(ui->editSshPassword, SessionTreeItem::SshPassword);
    sessionMapper->addMapping(ui->editSshKeyPath, SessionTreeItem::SshPrivateKey);
    sessionMapper->addMapping(ui->spinSshLocalPort, SessionTreeItem::SshLocalPort);
#if (defined (_WIN32) || defined (_WIN64))
    sessionMapper->addMapping(ui->editSshPlinkPath, SessionTreeItem::SshPlinkExe);
    sessionMapper->addMapping(ui->spinPlinkTimeout, SessionTreeItem::SshPlinkTimeout);
#endif

    // Advances settings tab
    sessionMapper->addMapping(ui->grpBoxSsl, SessionTreeItem::WantSsl);
    sessionMapper->addMapping(ui->editSslPrivateKeyPath, SessionTreeItem::SslPrivateKey);
    sessionMapper->addMapping(ui->editCaCertificatePath, SessionTreeItem::SslCaCertificate);
    sessionMapper->addMapping(ui->editCertificatePath, SessionTreeItem::SslCertificate);
    sessionMapper->addMapping(ui->editSslCipher, SessionTreeItem::SslCipher);

    sessionMapper->addMapping(ui->editStartupScriptPath, SessionTreeItem::StartupScriptFilename);
    sessionMapper->addMapping(ui->spinQueryTimeout, SessionTreeItem::QueryTimeOut);
    sessionMapper->addMapping(ui->spinPingTimeout, SessionTreeItem::PingTimeOut);
    sessionMapper->addMapping(ui->chkClientTimeZone, SessionTreeItem::ClientTimeZone);
    sessionMapper->addMapping(ui->chkFullTableStatus, SessionTreeItem::FullTableStatus);

    // Statistics tab
    sessionMapper->addMapping(ui->lblCreated, SessionTreeItem::SessionCreated);
    sessionMapper->addMapping(ui->lblLastConnect, SessionTreeItem::LastConnect);
    sessionMapper->addMapping(ui->lblSuccessConnects, SessionTreeItem::ConnectCount, "text");
    sessionMapper->addMapping(ui->lblUnsuccessConnects, SessionTreeItem::RefusedCount, "text");

    connect(ui->sessionsTreeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &SessionManager::sessionSelectionChanged);

    ui->actionSave->setEnabled(true);

    connect(sessionMapper, &IsDirtyDataWidgetMapper::dirtyStateChanged, this, [=](const bool isDirty) {
        qDebug() << "Is Dirty" << isDirty;

        ui->actionSave->setEnabled(isDirty);

        sessionsModel->setDirty(mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex()), isDirty);
    });
}

void SessionManager::sessionSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    // Remove tabs and add only needed for selected item
    while (ui->tabWidget->count() > 0) {
        ui->tabWidget->removeTab(0);
    }

    QModelIndex index = mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());
    sessionMapper->setRootIndex(index.parent());
    sessionMapper->setCurrentModelIndex(index);

    TreeItem *item = sessionsModel->getItem(index);

    if (index.isValid()) {
        ui->actionRename->setEnabled(true);
        ui->actionDelete->setEnabled(true);
    } else {
        ui->actionRename->setEnabled(false);
        ui->actionDelete->setEnabled(false);
    }

    if (index.isValid() && item->canEdit()) {
        ui->btnOpenConnection->setEnabled(true);
        ui->btnTestConnection->setEnabled(true);

        ui->tabWidget->addTab(ui->tabSettings, QIcon(":/icons/wrench.png"), QString(tr("Settings")));

        ui->tabWidget->addTab(ui->tabAdvancedOptions, QIcon(":/icons/wrench_orange.png"), QString(tr("Advanced")));
        ui->tabWidget->addTab(ui->tabStatistics, QIcon(":/icons/chart_bar.png"), QString(tr("Statistics")));
    } else {
        ui->btnOpenConnection->setEnabled(false);
        ui->btnTestConnection->setEnabled(false);

        ui->tabWidget->addTab(ui->tabStart, QIcon(":/icons/star.png"), QString(tr("Start")));
    }

    // Actions enabled
    if (item->canEdit()) {
        ui->actionSave_as->setEnabled(true);
    } else {
        ui->actionSave->setEnabled(false);
        ui->actionSave_as->setEnabled(false);
    }

    connectionTypeChanged();
    loginPromptChanged();
}

void SessionManager::connectionTypeChanged()
{
    if (ui->cmbNetworkType->currentIndex() == SessionTreeItem::NetType::SshTunnel) {
        ui->tabWidget->insertTab(1, ui->tabSshTunnel, QIcon(":/icons/lock_blue.png"), QString(tr("SSH Tunnel")));
    } else {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabSshTunnel));
    }

    if (ui->cmbNetworkType->currentIndex() == SessionTreeItem::NetType::LocalSocket) {
        ui->spinPort->setEnabled(false);
        ui->lblPort->setEnabled(false);
    } else {
        ui->spinPort->setEnabled(true);
        ui->lblPort->setEnabled(true);
    }
}

void SessionManager::loginPromptChanged()
{
    if (ui->chkCredentials->isChecked()) {
        ui->lblUsername->setEnabled(false);
        ui->editUsername->setEnabled(false);
        ui->lblPassword->setEnabled(false);
        ui->editPassword->setEnabled(false);
    } else {
        ui->lblUsername->setEnabled(true);
        ui->editUsername->setEnabled(true);
        ui->lblPassword->setEnabled(true);
        ui->editPassword->setEnabled(true);
    }
}

void SessionManager::on_btnCancel_clicked()
{
    close();
}

void SessionManager::selectForRename(const QModelIndex &index)
{
    ui->sessionsTreeView->setCurrentIndex(mapFromSource(index));
    ui->sessionsTreeView->selectionModel()->select(mapFromSource(index), QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    ui->sessionsTreeView->edit(mapFromSource(index));
}

void SessionManager::on_actionNew_folder_triggered()
{
    QModelIndex selectedIndex = mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());

    QModelIndex created = sessionsModel->createFolder(QString(tr("New Unnamed Folder")), selectedIndex);

    selectForRename(created);
}

void SessionManager::on_actionNew_session_triggered()
{
    QModelIndex selectedIndex = mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());

    QModelIndex created = sessionsModel->createSession(QString(tr("New Unnamed Session")), selectedIndex);

    selectForRename(created);
}

void SessionManager::on_actionRename_triggered()
{
    ui->sessionsTreeView->edit(ui->sessionsTreeView->selectionModel()->currentIndex());
}

void SessionManager::on_actionSave_triggered()
{
    sessionMapper->submit();
    sessionsModel->saveModelData();
}

void SessionManager::on_actionDelete_triggered()
{
    QModelIndex index = mapToSource(ui->sessionsTreeView->selectionModel()->currentIndex());
    TreeItem *item = sessionsModel->getItem(index);

    QMessageBox confirm;
    confirm.setWindowTitle(QString(tr("Confirm")));
    confirm.setText(QString(tr("Are really want to delete session \"%1\"?").arg(item->name())));
    confirm.setIcon(QMessageBox::Question);
    confirm.setStandardButtons(QMessageBox::Yes | QMessageBox::Cancel);
    confirm.setDefaultButton(QMessageBox::Yes);

    if (confirm.exec() == QMessageBox::Yes) {
        // Delete session
        if (sessionsModel->deleteSession(index)) {
            sessionsModel->saveModelData();
        };
    } else {
        confirm.close();
    };
}

void SessionManager::on_btnTestConnection_clicked()
{
    SessionTreeItem *item = static_cast<SessionTreeItem*>(selectedItem());

    DbConnection connection(item);

    connect(&connection, &DbConnection::log, this, &SessionManager::log);

    QMessageBox result;
    result.setWindowTitle(QString(tr("Connection testing")));
    result.setStandardButtons(QMessageBox::Ok);
    result.setDefaultButton(QMessageBox::Ok);

    QString connectionInfo = QString(tr("Connection parameters:\nHost: %1\nPort: %2\nUser: %3\nDatabase: %4"))
            .arg(connection.db.hostName())
            .arg(connection.db.port())
            .arg(connection.db.userName())
            .arg(connection.db.databaseName());

    if (connection.connect()) {
        result.setIcon(QMessageBox::Information);
        result.setText(QString(tr("Successfull connected to server\n\n%1"))
                       .arg(connectionInfo));
    } else {
        result.setIcon(QMessageBox::Critical);

        QSqlError error = connection.db.lastError();

        result.setText(QString(tr("Error connection to server:\n\n(%1): %2.\n\n%3"))
                       .arg(error.nativeErrorCode())
                       .arg(error.databaseText())
                       .arg(connectionInfo));
    }

    result.exec();
    connection.disconnect();
}

void SessionManager::on_btnOpenConnection_clicked()
{
    SessionTreeItem *item = static_cast<SessionTreeItem*>(selectedItem());
    DbConnection *connection = new DbConnection(item);

    connect(connection, &DbConnection::log, this, &SessionManager::log);

    if (connection->connect()) {
        emit connected(connection);

        close();
    } else {
        qWarning() << "Error connecting database";
    }
}

void SessionManager::on_sessionsTreeView_doubleClicked(const QModelIndex &index)
{
    on_btnOpenConnection_clicked();
}
