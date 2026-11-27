// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/catalog.h"
#include "owl/core/connectionform.h"
#include "owl/core/dberror.h"
#include "owl/core/dialect.h"
#include "owl/core/result.h"
#include "owl/core/resultset.h"
#include "owl/core/value.h"

#include <QList>
#include <QString>
#include <QVariantMap>
#include <QtPlugin>

#include <chrono>
#include <memory>
#include <optional>

// Driver plugin interfaces (docs/specs/driver-api.md §3).
//
// Threading contract: an IConnection and the cursors it returns are used from exactly one thread
// (its ConnectionWorker). ICancelHandle::cancel() is the only call allowed from other threads.
// ICatalog (owl/core/catalog.h) belongs to its connection and follows the same rule.

namespace owl {

// Bumped whenever these interfaces change incompatibly. Plugins declare the version they were
// built against in their metadata ("coreApi") and are rejected on mismatch (decision 049).
// 2: IDriver::dialect(), 3: IConnection::catalog(), 4: IDriver::connectionForm() / parseUrl()
inline constexpr int coreApiVersion = 4;

struct SslOptions
{
    enum class Mode : quint8 { Disable, Prefer, Require, VerifyCa, VerifyFull };
    Mode mode = Mode::Prefer;
    QString caFile;
    QString certFile;
    QString keyFile;

    bool operator==(const SslOptions &) const = default;
};

// "disable", "prefer", "require", "verify-ca", "verify-full" (libpq's names; URLs, forms, JSON).
QString sslModeName(SslOptions::Mode mode);
std::optional<SslOptions::Mode> sslModeFromName(QStringView name);

struct ConnectionParams
{
    enum class Transport : quint8 { Tcp, Socket };

    QString driverId;
    Transport transport = Transport::Tcp;
    QString host;
    quint16 port = 0;
    QString socketPath;
    QString user;
    QString password; // resolved from the keychain by the app layer just before open()
    QString database;
    SslOptions ssl;
    QVariantMap driverOptions; // per-driver extras
    QString applicationName = QStringLiteral("OwlSql");
    std::chrono::seconds connectTimeout{15};

    bool operator==(const ConnectionParams &) const = default;
};

struct ServerInfo
{
    QString product; // "PostgreSQL", "MariaDB", "MySQL", "SQLite"
    QString version; // as reported, e.g. "17.2"
    int versionNumber = 0; // comparable number, e.g. 170002
};

enum class Capability : quint8 {
    Schemas,
    MultipleDatabases,
    DatabaseSwitchInSession,
    TransactionalDdl,
    Returning,
    MultipleResultSets,
    ServerSideCancel,
};

class Capabilities
{
public:
    Capabilities() = default;
    Capabilities(std::initializer_list<Capability> list)
    {
        for (const Capability c : list)
            m_bits |= bit(c);
    }
    bool has(Capability c) const { return (m_bits & bit(c)) != 0; }

private:
    static quint64 bit(Capability c) { return quint64{1} << static_cast<int>(c); }
    quint64 m_bits = 0;
};

enum class StatementOrigin : quint8 { User, Internal, Browse }; // for the Log panel

struct Statement
{
    QString sql;
    QList<Value> params; // bound positionally; empty = simple query
    StatementOrigin origin = StatementOrigin::User;
};

struct ExecOptions
{
    std::optional<std::chrono::milliseconds> timeout;
    qsizetype fetchChunk = 1000; // rows delivered before a QueryJob pauses
    int previewLimitBytes = 64 * 1024; // large values are truncated to this (§6.3)
    bool readOnly = false;
};

enum class TxState : quint8 { Idle, InTransaction, Failed };

class IResultCursor
{
public:
    virtual ~IResultCursor() = default;
    // Columns of the current result; empty for statements that return no rows.
    virtual QList<ColumnInfo> columns() const = 0;
    // Up to maxRows rows; an empty batch means the current result is exhausted.
    virtual Result<RowBatch> fetch(qsizetype maxRows) = 0;
    virtual bool atEnd() const = 0;
    virtual qint64 affectedRows() const = 0; // -1 when not applicable
    virtual std::optional<Value> lastInsertId() const { return std::nullopt; }
    // Advances to the next result set (e.g. MySQL CALL); false when there is none.
    virtual bool nextResult() { return false; }
    // Discards the rest; may cancel or drain on the server.
    virtual void close() = 0;
};

class ICancelHandle
{
public:
    virtual ~ICancelHandle() = default;
    // Thread-safe: asks the server/library to abort the statement running on the connection.
    virtual Result<void> cancel() = 0;
};

class IConnection
{
public:
    virtual ~IConnection() = default;

    virtual Result<void> open(const ConnectionParams &params) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool ping() = 0;
    virtual ServerInfo serverInfo() const = 0;

    // A thread-safe handle; valid for the lifetime of the connection.
    virtual std::shared_ptr<ICancelHandle> cancelHandle() = 0;

    virtual Result<std::unique_ptr<IResultCursor>> execute(const Statement &statement,
                                                           const ExecOptions &options)
        = 0;

    virtual Result<void> setAutocommit(bool on) = 0;
    virtual TxState txState() const = 0;

    // Notices/warnings produced since the last call.
    virtual QList<ServerMessage> takeMessages() = 0;

    // Introspection over this connection; valid while the connection object lives.
    virtual ICatalog &catalog() = 0;
};

class IDriver
{
public:
    virtual ~IDriver() = default;
    virtual QString id() const = 0; // "postgres", "mysql", "sqlite"
    virtual QString displayName() const = 0; // "PostgreSQL"
    virtual Capabilities capabilities() const = 0;
    // The engine's SQL knowledge; valid for the driver's lifetime, usable from any thread.
    virtual const IDialect &dialect() const = 0;
    // Fields of the connection dialog (connectionform.h, decision 166).
    virtual ConnectionFormSchema connectionForm() const = 0;
    // A connection URL or string this driver understands ("postgresql://…", "host=… dbname=…",
    // "mysql://…", a SQLite file path) as ConnectionParams, including a password if the URL has
    // one. Unsupported when the text is not for this driver.
    virtual Result<ConnectionParams> parseUrl(const QString &url) const = 0;
    virtual std::unique_ptr<IConnection> createConnection() const = 0;
};

} // namespace owl

#define OWL_DRIVER_IID "org.owlsql.Driver/1"
Q_DECLARE_INTERFACE(owl::IDriver, OWL_DRIVER_IID)
