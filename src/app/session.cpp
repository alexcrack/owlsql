// SPDX-License-Identifier: GPL-3.0-or-later
#include "session.h"

#include "secretstore.h"

namespace owl::app {

Session::Session(SessionInfo info, IDriver *driver, SecretStore *secrets, QObject *parent)
    : QObject(parent)
    , m_info(std::move(info))
    , m_driver(driver)
    , m_secrets(secrets)
{ }

Session::~Session()
{
    // Catalogs before their workers.
    for (auto &[name, db] : m_databases)
        db.catalog.reset();
    m_metaCatalog.reset();
}

ServerInfo Session::serverInfo() const
{
    return m_meta ? m_meta->serverInfo() : ServerInfo{};
}

void Session::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void Session::connectToServer()
{
    if (m_state == State::Connecting || m_state == State::Connected)
        return;
    const quint64 attempt = ++m_attempt;
    m_lastError = {};
    switch (m_info.secretMode(secret::Password)) {
    case SecretMode::None:
        m_password.clear();
        openMeta();
        return;
    case SecretMode::Ask:
        setState(State::NeedsPassword);
        emit passwordRequired();
        return;
    case SecretMode::Keychain:
        break;
    }
    if (!m_secrets || !m_secrets->isAvailable()) {
        setState(State::NeedsPassword); // no keychain on this system: ask instead
        emit passwordRequired();
        return;
    }
    setState(State::Connecting);
    m_secrets->read(m_info.id, secret::Password,
                    [this, self = QPointer(this), attempt](const SecretStore::Outcome &o) {
                        if (!self || attempt != m_attempt || m_state != State::Connecting)
                            return;
                        if (!o.ok) { // not stored (any more) or the keychain refused: ask
                            setState(State::NeedsPassword);
                            emit passwordRequired();
                            return;
                        }
                        m_password = o.value;
                        openMeta();
                    });
}

void Session::providePassword(const QString &password, bool remember)
{
    if (m_state != State::NeedsPassword)
        return;
    m_password = password;
    if (remember && m_secrets && m_secrets->isAvailable()) {
        m_secrets->write(m_info.id, secret::Password, password);
        m_info.secretModes.insert(secret::Password, SecretMode::Keychain);
        emit infoChanged();
    }
    openMeta();
}

ConnectionParams Session::paramsFor(const QString &database) const
{
    ConnectionParams p = m_info.params;
    p.driverId = m_info.driverId;
    p.password = m_password;
    if (!database.isEmpty())
        p.database = database;
    return p;
}

void Session::openMeta()
{
    setState(State::Connecting);
    m_meta = std::make_unique<ConnectionWorker>(m_driver);
    m_metaCatalog = std::make_unique<CatalogCache>(m_meta.get());
    ConnectionWorker *worker = m_meta.get();
    connect(worker, &ConnectionWorker::opened, this, [this] {
        setState(State::Connected);
        emit connected();
    });
    connect(worker, &ConnectionWorker::openFailed, this, [this](const DbError &error) {
        m_lastError = error;
        m_metaCatalog.reset();
        m_meta.reset();
        m_password.clear();
        setState(State::Failed);
        emit failed(error);
    });
    worker->open(paramsFor({}), startupStatements());
}

void Session::disconnectFromServer()
{
    ++m_attempt;
    for (auto &[name, db] : m_databases)
        db.catalog.reset();
    m_databases.clear();
    m_metaCatalog.reset();
    m_meta.reset(); // closes the connection on its thread
    m_password.clear();
    if (m_state != State::Disconnected) {
        setState(State::Disconnected);
        emit disconnected();
    }
}

bool Session::usesDatabaseConnections() const
{
    const Capabilities caps = m_driver->capabilities();
    return caps.has(Capability::MultipleDatabases) && !caps.has(Capability::DatabaseSwitchInSession)
        && !m_info.options.singleConnection;
}

ConnectionWorker *Session::workerFor(const QString &database)
{
    if (m_state != State::Connected)
        return nullptr;
    if (database.isEmpty() || database == m_info.params.database || !usesDatabaseConnections())
        return m_meta.get();
    Database &db = m_databases[database];
    if (!db.worker) {
        db.worker = std::make_unique<ConnectionWorker>(m_driver);
        db.catalog = std::make_unique<CatalogCache>(db.worker.get());
        db.worker->open(paramsFor(database), startupStatements());
    }
    return db.worker.get();
}

CatalogCache *Session::catalogFor(const QString &database)
{
    ConnectionWorker *worker = workerFor(database);
    if (!worker)
        return nullptr;
    if (worker == m_meta.get())
        return m_metaCatalog.get();
    return m_databases.at(database).catalog.get();
}

QStringList Session::startupStatements() const
{
    const QString script = m_info.options.startupScript;
    QStringList out;
    for (const StatementSpan &s : m_driver->dialect().split(script))
        out << s.in(script).toString();
    return out;
}

ExecOptions Session::defaultExecOptions() const
{
    ExecOptions o;
    o.readOnly = m_info.options.readOnly;
    if (m_info.options.fetchSize)
        o.fetchChunk = *m_info.options.fetchSize;
    if (m_info.options.queryTimeoutSec)
        o.timeout = std::chrono::seconds(*m_info.options.queryTimeoutSec);
    return o;
}

bool Session::isDatabaseVisible(const QString &database) const
{
    return m_info.options.visibleDatabases.isEmpty()
        || m_info.options.visibleDatabases.contains(database);
}

} // namespace owl::app
