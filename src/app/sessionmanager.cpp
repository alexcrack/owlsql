// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionmanager.h"

#include "appcontext.h"
#include "owl/core/connectionworker.h"
#include "owl/core/driverregistry.h"
#include "secretstore.h"
#include "session.h"
#include "sessionlistmodel.h"
#include "sessionstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace owl::app {

namespace {

QString typeName(FormField::Type type)
{
    switch (type) {
    case FormField::Type::Password:
        return QStringLiteral("password");
    case FormField::Type::Integer:
        return QStringLiteral("integer");
    case FormField::Type::Bool:
        return QStringLiteral("bool");
    case FormField::Type::Choice:
        return QStringLiteral("choice");
    case FormField::Type::File:
        return QStringLiteral("file");
    case FormField::Type::Directory:
        return QStringLiteral("directory");
    case FormField::Type::Multiline:
        return QStringLiteral("multiline");
    case FormField::Type::Text:
        break;
    }
    return QStringLiteral("text");
}

QString stateName(Session::State state)
{
    switch (state) {
    case Session::State::Connecting:
        return QStringLiteral("connecting");
    case Session::State::NeedsPassword:
        return QStringLiteral("needsPassword");
    case Session::State::Connected:
        return QStringLiteral("connected");
    case Session::State::Failed:
        return QStringLiteral("failed");
    case Session::State::Disconnected:
        break;
    }
    return {};
}

QVariantMap result(bool ok, const QString &error = {})
{
    QVariantMap m{{QStringLiteral("ok"), ok}};
    if (!error.isEmpty())
        m.insert(QStringLiteral("error"), error);
    return m;
}

} // namespace

SessionManager::SessionManager(SessionStore *store, DriverRegistry *drivers, SecretStore *secrets,
                               QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_drivers(drivers)
    , m_secrets(secrets)
    , m_model(std::make_unique<SessionListModel>(this))
{
    if (m_store) {
        const auto refresh = [this] {
            m_model->setContent(m_store->folders(), m_store->sessions());
            emit foldersChanged();
        };
        connect(m_store, &SessionStore::changed, this, refresh);
        refresh();
    }
}

SessionManager::~SessionManager()
{
    // Sessions (children) close their connections; the model goes before them.
    m_model.reset();
}

SessionManager *SessionManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    SessionManager *manager = AppContext::current()->sessionManager();
    QJSEngine::setObjectOwnership(manager, QJSEngine::CppOwnership);
    return manager;
}

QVariantList SessionManager::drivers() const
{
    // Tier-1 engines first, in a fixed order; other drivers after them.
    static const QStringList preferred{QStringLiteral("postgres"), QStringLiteral("mysql"),
                                       QStringLiteral("sqlite")};
    QList<IDriver *> all = m_drivers->drivers();
    std::ranges::stable_sort(all, {}, [](const IDriver *d) {
        const qsizetype i = preferred.indexOf(d->id());
        return i < 0 ? preferred.size() : i;
    });
    QVariantList out;
    for (IDriver *d : all)
        out.append(QVariantMap{{QStringLiteral("id"), d->id()},
                               {QStringLiteral("name"), d->displayName()}});
    return out;
}

QVariantList SessionManager::folders() const
{
    QVariantList out;
    if (!m_store)
        return out;
    const QList<FolderInfo> all = m_store->folders();
    std::function<void(const QString &, int)> add = [&](const QString &parent, int depth) {
        for (const FolderInfo &f : all) {
            if (f.parentId != parent)
                continue;
            out.append(QVariantMap{{QStringLiteral("id"), f.id},
                                   {QStringLiteral("name"), f.name},
                                   {QStringLiteral("depth"), depth}});
            add(f.id, depth + 1);
        }
    };
    add({}, 0);
    return out;
}

bool SessionManager::keychainAvailable() const
{
    return m_secrets && m_secrets->isAvailable();
}

void SessionManager::setActiveSession(const QString &id)
{
    if (id == m_active)
        return;
    m_active = id;
    emit activeSessionChanged();
}

IDriver *SessionManager::driverFor(const QString &driverId) const
{
    return m_drivers ? m_drivers->driver(driverId) : nullptr;
}

QVariantList SessionManager::form(const QString &driverId) const
{
    QVariantList out;
    IDriver *driver = driverFor(driverId);
    if (!driver)
        return out;
    for (const FormField &f : driver->connectionForm().fields) {
        QVariantList choices;
        for (const FormChoice &c : f.choices)
            choices.append(QVariantMap{{QStringLiteral("value"), c.value},
                                       {QStringLiteral("label"), c.label}});
        out.append(QVariantMap{
            {QStringLiteral("key"), f.key},
            {QStringLiteral("type"), typeName(f.type)},
            {QStringLiteral("label"), f.label},
            {QStringLiteral("group"), f.group},
            {QStringLiteral("default"), f.defaultValue},
            {QStringLiteral("choices"), choices},
            {QStringLiteral("required"), f.required},
            {QStringLiteral("minimum"), f.minimum},
            {QStringLiteral("maximum"), f.maximum},
            {QStringLiteral("placeholder"), f.placeholder},
            {QStringLiteral("help"), f.help},
            {QStringLiteral("visibleWhenKey"), f.visibleWhenKey},
            {QStringLiteral("visibleWhenValue"), f.visibleWhenValue},
        });
    }
    return out;
}

QVariantMap SessionManager::toDraft(const SessionInfo &info, const QString &password) const
{
    QVariantMap values;
    if (IDriver *driver = driverFor(info.driverId)) {
        ConnectionParams p = info.params;
        p.password = password;
        values = formValues(driver->connectionForm(), p);
    }
    const SecretMode mode = info.secretMode(secret::Password);
    return {{QStringLiteral("id"), info.id},
            {QStringLiteral("name"), info.name},
            {QStringLiteral("driverId"), info.driverId},
            {QStringLiteral("folderId"), info.folderId},
            {QStringLiteral("level"), safetyLevelName(info.safetyLevel)},
            {QStringLiteral("tag"), info.color},
            {QStringLiteral("notes"), info.notes},
            {QStringLiteral("startupScript"), info.options.startupScript},
            {QStringLiteral("readOnly"), info.options.readOnly},
            {QStringLiteral("savePassword"), mode != SecretMode::Ask && keychainAvailable()},
            {QStringLiteral("hasStoredPassword"), mode == SecretMode::Keychain},
            {QStringLiteral("values"), values}};
}

QVariantMap SessionManager::newDraft(const QString &driverId, const QString &folderId) const
{
    SessionInfo info;
    info.driverId = driverId;
    info.folderId = folderId;
    info.secretModes.insert(secret::Password, SecretMode::None);
    QVariantMap draft = toDraft(info);
    draft.insert(QStringLiteral("savePassword"), keychainAvailable());
    return draft;
}

QVariantMap SessionManager::draft(const QString &sessionId) const
{
    if (!m_store)
        return {};
    const std::optional<SessionInfo> info = m_store->session(sessionId);
    return info ? toDraft(*info) : QVariantMap();
}

QVariantMap SessionManager::draftFromUrl(const QString &url, const QString &folderId) const
{
    const Result<ImportBundle> bundle = importFromUrl(*m_drivers, url);
    if (!bundle)
        return {{QStringLiteral("error"), bundle.error().message}};
    const ImportedSession &s = bundle->sessions.first();
    SessionInfo info = s.info;
    info.folderId = folderId;
    QVariantMap draft = toDraft(info, s.password);
    draft.insert(QStringLiteral("savePassword"), keychainAvailable());
    return draft;
}

Result<SessionInfo> SessionManager::infoFromDraft(const QVariantMap &draft, QString *password) const
{
    const QString driverId = draft.value(QStringLiteral("driverId")).toString();
    IDriver *driver = driverFor(driverId);
    if (!driver)
        return fail(DbError::Category::Unsupported, tr("Unknown driver \"%1\"").arg(driverId));
    SessionInfo info;
    const QString id = draft.value(QStringLiteral("id")).toString();
    if (!id.isEmpty() && m_store) {
        if (std::optional<SessionInfo> existing = m_store->session(id))
            info = *existing; // keeps what the form does not edit
    }
    Result<ConnectionParams> params = paramsFromForm(driver->connectionForm(), driverId,
                                                     draft.value(QStringLiteral("values")).toMap());
    if (!params)
        return fail(params.error());
    *password = params->password;
    params->password.clear();
    info.id = id;
    info.driverId = driverId;
    info.params = *params;
    info.name = draft.value(QStringLiteral("name")).toString().trimmed();
    info.folderId = draft.value(QStringLiteral("folderId")).toString();
    info.safetyLevel = safetyLevelFromName(draft.value(QStringLiteral("level")).toString());
    info.color = draft.value(QStringLiteral("tag")).toString();
    info.notes = draft.value(QStringLiteral("notes")).toString();
    info.options.startupScript = draft.value(QStringLiteral("startupScript")).toString();
    info.options.readOnly = draft.value(QStringLiteral("readOnly")).toBool();
    return info;
}

QVariantMap SessionManager::validate(const QVariantMap &draft) const
{
    QVariantMap errors;
    if (draft.value(QStringLiteral("name")).toString().trimmed().isEmpty())
        errors.insert(QStringLiteral("name"), tr("Give the session a name"));
    IDriver *driver = driverFor(draft.value(QStringLiteral("driverId")).toString());
    if (!driver) {
        errors.insert(QStringLiteral("driverId"), tr("Choose a database engine"));
        return errors;
    }
    QList<FieldError> problems;
    (void)paramsFromForm(driver->connectionForm(), driver->id(),
                         draft.value(QStringLiteral("values")).toMap(), &problems);
    for (const FieldError &e : problems)
        errors.insert(e.key, e.message);
    return errors;
}

QVariantMap SessionManager::save(const QVariantMap &draft)
{
    if (!m_store)
        return result(false, tr("Sessions cannot be saved: the app database is not available"));
    if (const QVariantMap errors = validate(draft); !errors.isEmpty())
        return result(false, errors.first().toString());
    QString password;
    Result<SessionInfo> info = infoFromDraft(draft, &password);
    if (!info)
        return result(false, info.error().message);

    const bool hasPasswordField = driverFor(info->driverId)->connectionForm().field(u"password");
    const bool wasStored = info->secretMode(secret::Password) == SecretMode::Keychain;
    SecretMode mode = SecretMode::None;
    if (hasPasswordField) {
        if (draft.value(QStringLiteral("savePassword")).toBool() && keychainAvailable()) {
            // Typed: store it. Blank: keep what is stored, or none (trust / peer login).
            mode = !password.isEmpty() || wasStored ? SecretMode::Keychain : SecretMode::None;
        } else {
            mode = SecretMode::Ask;
        }
    }
    info->secretModes.insert(secret::Password, mode);
    const Result<SessionInfo> saved = m_store->saveSession(*info);
    if (!saved)
        return result(false, saved.error().message);
    if (mode == SecretMode::Keychain && !password.isEmpty())
        m_secrets->write(saved->id, secret::Password, password);
    else if (mode != SecretMode::Keychain && wasStored && m_secrets)
        m_secrets->remove(saved->id, secret::Password);
    QVariantMap out = result(true);
    out.insert(QStringLiteral("id"), saved->id);
    return out;
}

bool SessionManager::remove(const QString &sessionId)
{
    if (!m_store)
        return false;
    close(sessionId);
    if (!m_store->removeSession(sessionId))
        return false;
    if (m_secrets)
        m_secrets->removeAll(sessionId);
    return true;
}

QString SessionManager::duplicate(const QString &sessionId)
{
    if (!m_store)
        return {};
    const Result<SessionInfo> copy = m_store->duplicateSession(sessionId);
    return copy ? copy->id : QString();
}

QString SessionManager::addFolder(const QString &name, const QString &parentId)
{
    if (!m_store || name.trimmed().isEmpty())
        return {};
    const Result<FolderInfo> folder = m_store->addFolder(name.trimmed(), parentId);
    return folder ? folder->id : QString();
}

bool SessionManager::renameFolder(const QString &folderId, const QString &name)
{
    if (!m_store || name.trimmed().isEmpty())
        return false;
    std::optional<FolderInfo> f = m_store->folder(folderId);
    if (!f)
        return false;
    f->name = name.trimmed();
    return m_store->updateFolder(*f).has_value();
}

bool SessionManager::removeFolder(const QString &folderId)
{
    return m_store && m_store->removeFolder(folderId).has_value();
}

void SessionManager::setFolderExpanded(const QString &folderId, bool expanded)
{
    if (!m_store)
        return;
    std::optional<FolderInfo> f = m_store->folder(folderId);
    if (!f || f->expanded == expanded)
        return;
    f->expanded = expanded;
    (void)m_store->updateFolder(*f);
}

bool SessionManager::moveSession(const QString &sessionId, const QString &folderId, int position)
{
    return m_store && m_store->moveSession(sessionId, folderId, position).has_value();
}

QString SessionManager::localPath(const QUrl &url)
{
    return url.isLocalFile() ? QDir::toNativeSeparators(url.toLocalFile()) : url.toString();
}

// --- Test connection ---------------------------------------------------------------------------

void SessionManager::test(const QVariantMap &draft)
{
    cancelTest();
    QString password;
    Result<SessionInfo> info = infoFromDraft(draft, &password);
    if (!info) {
        emit testFinished(false, info.error().message);
        return;
    }
    IDriver *driver = driverFor(info->driverId);
    ConnectionParams params = info->params;
    params.password = password;
    QStringList startup;
    for (const StatementSpan &s : driver->dialect().split(info->options.startupScript))
        startup << s.in(info->options.startupScript).toString();

    m_testWorker = std::make_unique<ConnectionWorker>(driver);
    ConnectionWorker *worker = m_testWorker.get();
    connect(worker, &ConnectionWorker::opened, this, [this, worker] {
        const ServerInfo server = worker->serverInfo();
        finishTest(true, tr("Connected to %1 %2").arg(server.product, server.version));
    });
    connect(worker, &ConnectionWorker::openFailed, this,
            [this](const DbError &error) { finishTest(false, error.toString()); });
    emit testingChanged();

    // A blank password on a session that has one stored: test with the stored one.
    const bool useStored = password.isEmpty() && !info->id.isEmpty() && m_secrets
        && info->secretMode(secret::Password) == SecretMode::Keychain;
    if (!useStored) {
        worker->open(params, startup);
        return;
    }
    m_secrets->read(
        info->id, secret::Password,
        [this, worker = QPointer(worker), params, startup](const SecretStore::Outcome &o) mutable {
            if (!worker || worker != m_testWorker.get())
                return;
            params.password = o.value;
            worker->open(params, startup);
        });
}

void SessionManager::finishTest(bool ok, const QString &message)
{
    if (m_testWorker)
        m_testWorker.release()->deleteLater(); // we are inside its signal
    emit testingChanged();
    emit testFinished(ok, message);
}

void SessionManager::cancelTest()
{
    if (!m_testWorker)
        return;
    // The connect may block for its timeout: let it end on its own and drop the answer.
    ConnectionWorker *worker = m_testWorker.release();
    disconnect(worker, nullptr, this, nullptr);
    connect(worker, &ConnectionWorker::opened, worker, &QObject::deleteLater);
    connect(worker, &ConnectionWorker::openFailed, worker, &QObject::deleteLater);
    emit testingChanged();
}

// --- Import / export ---------------------------------------------------------------------------

QVariantMap SessionManager::inspectImport(const QUrl &file)
{
    m_pendingImport.reset();
    const QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return result(false, tr("Cannot read %1: %2").arg(path, f.errorString()));
    const QByteArray bytes = f.readAll();
    const QString name = QFileInfo(path).fileName().toLower();
    const QString text = QString::fromUtf8(bytes);

    QString source;
    ImportBundle bundle;
    static const QRegularExpression pgpassLine(QStringLiteral("^[^#\\n]*(?::[^:\\n]*){4}$"),
                                               QRegularExpression::MultilineOption);
    if (name.endsWith(u".json") || text.trimmed().startsWith(u'{')) {
        Result<ImportBundle> json = importSessionsJson(bytes);
        if (!json)
            return result(false, json.error().message);
        bundle = *json;
        source = QStringLiteral("json");
    } else if (name.contains(u"pgpass")
               || (!text.contains(u'[') && pgpassLine.match(text).hasMatch())) {
        bundle = importPgpass(text);
        source = QStringLiteral("pgpass");
    } else if (name.endsWith(u".cnf") || name.endsWith(u".ini") || text.contains(u"[client")
               || text.contains(u"[mysql]")) {
        bundle = importMyCnf(text);
        source = QStringLiteral("mycnf");
    } else {
        return result(false, tr("Not a sessions file, .pgpass or .my.cnf"));
    }

    int conflicts = 0;
    for (const ImportedSession &s : std::as_const(bundle.sessions)) {
        if (!s.info.id.isEmpty() && m_store && m_store->session(s.info.id))
            ++conflicts;
    }
    QVariantMap out = result(true);
    out.insert(QStringLiteral("source"), source);
    out.insert(QStringLiteral("sessions"), static_cast<int>(bundle.sessions.size()));
    out.insert(QStringLiteral("conflicts"), conflicts);
    out.insert(QStringLiteral("warnings"), bundle.warnings);
    m_pendingImport = std::move(bundle);
    return out;
}

QVariantMap SessionManager::applyImport(const QString &policy)
{
    if (!m_store || !m_pendingImport)
        return result(false, tr("Nothing to import"));
    const ConflictPolicy p = policy == u"replace" ? ConflictPolicy::Replace
        : policy == u"skip"                       ? ConflictPolicy::Skip
                                                  : ConflictPolicy::KeepBoth;
    const Result<QList<SessionInfo>> written
        = owl::app::applyImport(*m_store, m_secrets, *m_pendingImport, p);
    m_pendingImport.reset();
    if (!written)
        return result(false, written.error().message);
    QVariantMap out = result(true);
    out.insert(QStringLiteral("imported"), static_cast<int>(written->size()));
    return out;
}

QVariantMap SessionManager::exportTo(const QUrl &file, const QStringList &sessionIds)
{
    if (!m_store)
        return result(false, tr("Nothing to export"));
    QList<SessionInfo> sessions;
    QSet<QString> folderIds;
    const QList<FolderInfo> allFolders = m_store->folders();
    for (const SessionInfo &s : m_store->sessions()) {
        if (!sessionIds.isEmpty() && !sessionIds.contains(s.id))
            continue;
        sessions.append(s);
        for (QString id = s.folderId; !id.isEmpty() && !folderIds.contains(id);) {
            folderIds.insert(id);
            const auto it
                = std::ranges::find_if(allFolders, [&](const FolderInfo &f) { return f.id == id; });
            id = it == allFolders.cend() ? QString() : it->parentId;
        }
    }
    QList<FolderInfo> folders;
    for (const FolderInfo &f : allFolders) {
        if (folderIds.contains(f.id))
            folders.append(f);
    }
    QFile out(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return result(false, tr("Cannot write %1: %2").arg(out.fileName(), out.errorString()));
    out.write(exportSessions(folders, sessions).toJson(QJsonDocument::Indented));
    QVariantMap r = result(true);
    r.insert(QStringLiteral("exported"), static_cast<int>(sessions.size()));
    return r;
}

// --- Open sessions -----------------------------------------------------------------------------

Session *SessionManager::session(const QString &sessionId) const
{
    return m_open.value(sessionId);
}

void SessionManager::open(const QString &sessionId)
{
    if (Session *s = session(sessionId)) {
        setActiveSession(sessionId);
        if (s->state() == Session::State::Failed || s->state() == Session::State::Disconnected)
            s->connectToServer();
        else if (s->state() == Session::State::NeedsPassword)
            emit passwordRequested(sessionId, s->info().name);
        return;
    }
    if (!m_store)
        return;
    const std::optional<SessionInfo> info = m_store->session(sessionId);
    if (!info) {
        emit sessionFailed(sessionId, tr("No such session"));
        return;
    }
    IDriver *driver = driverFor(info->driverId);
    if (!driver) {
        emit sessionFailed(sessionId, tr("The %1 driver is not available").arg(info->driverId));
        return;
    }
    auto *s = new Session(info.value(), driver, m_secrets, this);
    m_open.insert(sessionId, s);
    m_openOrder.append(sessionId);
    connect(s, &Session::stateChanged, this, [this, sessionId] {
        m_model->refreshStates();
        emit sessionStateChanged(sessionId);
        if (sessionId == m_active)
            emit activeSessionChanged();
    });
    connect(s, &Session::passwordRequired, this,
            [this, s, sessionId] { emit passwordRequested(sessionId, s->info().name); });
    connect(s, &Session::connected, this, [this, sessionId] {
        if (m_store)
            (void)m_store->markUsed(sessionId);
    });
    connect(s, &Session::failed, this, [this, sessionId](const DbError &error) {
        emit sessionFailed(sessionId, error.toString());
    });
    connect(s, &Session::infoChanged, this, [this, s] {
        if (m_store)
            (void)m_store->saveSession(s->info());
    });
    emit openSessionsChanged();
    setActiveSession(sessionId);
    m_model->refreshStates();
    s->connectToServer();
}

void SessionManager::providePassword(const QString &sessionId, const QString &password,
                                     bool remember) const
{
    if (Session *s = session(sessionId))
        s->providePassword(password, remember);
}

void SessionManager::close(const QString &sessionId)
{
    Session *s = m_open.take(sessionId);
    if (!s)
        return;
    m_openOrder.removeAll(sessionId);
    s->disconnect(this);
    s->disconnectFromServer();
    s->deleteLater();
    emit openSessionsChanged();
    if (m_active == sessionId)
        setActiveSession(m_openOrder.isEmpty() ? QString() : m_openOrder.last());
    m_model->refreshStates();
    emit sessionStateChanged(sessionId);
}

QString SessionManager::state(const QString &sessionId) const
{
    const Session *s = session(sessionId);
    return s ? stateName(s->state()) : QString();
}

QVariantMap SessionManager::info(const QString &sessionId) const
{
    if (sessionId.isEmpty())
        return {};
    SessionInfo info;
    QString error;
    ServerInfo server;
    if (const Session *s = session(sessionId)) {
        info = s->info();
        error = s->lastError().message;
        server = s->serverInfo();
    } else if (m_store) {
        if (std::optional<SessionInfo> stored = m_store->session(sessionId))
            info = *stored;
    }
    if (info.id.isEmpty())
        return {};
    const IDriver *driver = driverFor(info.driverId);
    return {
        {QStringLiteral("id"), info.id},
        {QStringLiteral("name"), info.name},
        {QStringLiteral("level"), safetyLevelName(info.safetyLevel)},
        {QStringLiteral("tag"), info.color.isEmpty() ? defaultTag(info.safetyLevel) : info.color},
        {QStringLiteral("state"), state(sessionId)},
        {QStringLiteral("error"), error},
        {QStringLiteral("product"), server.product},
        {QStringLiteral("version"), server.version},
        {QStringLiteral("driverName"), driver ? driver->displayName() : info.driverId}};
}

} // namespace owl::app
