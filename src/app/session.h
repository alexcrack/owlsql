// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/catalogcache.h"
#include "owl/core/connectionworker.h"
#include "sessioninfo.h"

#include <QHash>
#include <QObject>
#include <QPointer>

#include <map>
#include <memory>

namespace owl::app {

class SecretStore;

// A saved session while it is open (driver-api.md §6.1, decision 043): the `meta` connection
// with its catalog, plus — for engines that cannot switch databases in a session (PostgreSQL) —
// one connection and catalog per further database the user browses. Every physical connection
// runs the session's startup script. Main thread only.
//
//   Disconnected → (NeedsPassword →) Connecting → Connected
//                                              ↘ Failed
class Session : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged FINAL)

public:
    enum class State : quint8 { Disconnected, NeedsPassword, Connecting, Connected, Failed };
    Q_ENUM(State)

    // `driver` and `secrets` must outlive the session.
    Session(SessionInfo info, IDriver *driver, SecretStore *secrets, QObject *parent = nullptr);
    ~Session() override;

    const SessionInfo &info() const { return m_info; }
    IDriver *driver() const { return m_driver; }
    State state() const { return m_state; }
    DbError lastError() const { return m_lastError; }
    ServerInfo serverInfo() const;

    // Gets the password as the session's secret mode says (keychain / ask / none) and opens
    // the meta connection. In NeedsPassword, call providePassword().
    void connectToServer();
    // `remember`: also store it in the keychain (password mode becomes "keychain"; the caller
    // saves info() — infoChanged() tells it to).
    void providePassword(const QString &password, bool remember = false);
    void disconnectFromServer();

    ConnectionWorker *metaWorker() const { return m_meta.get(); }
    CatalogCache *catalog() const { return m_metaCatalog.get(); }
    // The catalog that can browse `database`: the meta one, or (PostgreSQL) one on its own
    // connection, opened on first use. Null while not connected.
    CatalogCache *catalogFor(const QString &database);
    ConnectionWorker *workerFor(const QString &database);
    // True when `database` gets its own connection (see class comment).
    bool usesDatabaseConnections() const;

    // The session's startup script, split into statements by the driver's dialect.
    QStringList startupStatements() const;
    // Statement defaults from the session options (read-only, fetch size).
    ExecOptions defaultExecOptions() const;
    // options.visibleDatabases is empty or lists it.
    bool isDatabaseVisible(const QString &database) const;

signals:
    void stateChanged();
    void passwordRequired(); // state NeedsPassword
    void connected();
    void failed(const owl::DbError &error);
    void disconnected();
    void infoChanged(); // the session now stores its password in the keychain: save info()

private:
    struct Database
    {
        std::unique_ptr<ConnectionWorker> worker;
        std::unique_ptr<CatalogCache> catalog;
    };

    void setState(State state);
    void openMeta();
    ConnectionParams paramsFor(const QString &database) const;

    SessionInfo m_info;
    IDriver *m_driver;
    QPointer<SecretStore> m_secrets;
    State m_state = State::Disconnected;
    DbError m_lastError;
    QString m_password; // kept for further connections while open
    std::unique_ptr<ConnectionWorker> m_meta;
    std::unique_ptr<CatalogCache> m_metaCatalog;
    std::map<QString, Database> m_databases;
    quint64 m_attempt = 0; // ignores keychain answers for an earlier connect
};

} // namespace owl::app
