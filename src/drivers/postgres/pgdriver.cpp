// SPDX-License-Identifier: GPL-3.0-or-later
#include "pgdriver.h"

#include "owl/core/formbuilder.h"
#include "pgcatalog.h"
#include "pgvalues.h"

#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>
#include <QThreadPool>

#include <array>
#include <libpq-fe.h>
#include <utility>
#include <vector>

namespace owl::postgres {

namespace {

// Rows per libpq chunk (PQsetChunkedRowsMode): matches the worker's batch size, so a fetch()
// usually consumes exactly one chunk and memory stays bounded however large the result is.
constexpr int chunkRows = 256;

struct ResultDeleter
{
    void operator()(PGresult *r) const { PQclear(r); }
};
using PgResult = std::unique_ptr<PGresult, ResultDeleter>;

// The cancel connection, shared with ICancelHandle (used from other threads).
struct CancelState
{
    QMutex mutex;
    PGcancelConn *cancel = nullptr;
};

// Blocking: opens a separate connection to the server and sends the cancel request.
void sendCancel(CancelState &state)
{
    QMutexLocker lock(&state.mutex);
    if (!state.cancel)
        return;
    (void)PQcancelBlocking(state.cancel); // failures are not actionable: the query just runs on
    PQcancelReset(state.cancel);
}

class PgCancel : public ICancelHandle
{
public:
    explicit PgCancel(std::shared_ptr<CancelState> state)
        : m_state(std::move(state))
    { }

    // Called on the UI thread: the network round trip happens on a pool thread.
    Result<void> cancel() override
    {
        QThreadPool::globalInstance()->start([state = m_state] { sendCancel(*state); });
        return {};
    }

private:
    std::shared_ptr<CancelState> m_state;
};

QString trimmed(const char *text)
{
    return QString::fromUtf8(text).trimmed();
}

// PostgreSQL error positions count characters (code points) from 1; editors need UTF-16 offsets.
int utf16Position(const QString &sql, int position)
{
    if (position <= 0)
        return -1;
    qsizetype codePoints = 0;
    for (qsizetype i = 0; i < sql.size(); ++i) {
        if (codePoints == position - 1)
            return static_cast<int>(i);
        if (sql.at(i).isHighSurrogate() && i + 1 < sql.size() && sql.at(i + 1).isLowSurrogate())
            ++i;
        ++codePoints;
    }
    return static_cast<int>(sql.size());
}

DbError::Category categoryFor(const QString &sqlState, const QString &message)
{
    using C = DbError::Category;
    if (sqlState == QLatin1String("57014")) // query_canceled (user cancel or statement_timeout)
        return message.contains(QLatin1String("timeout")) ? C::Timeout : C::Cancelled;
    if (sqlState == QLatin1String("42501") || sqlState == QLatin1String("25006"))
        return C::Permission; // insufficient_privilege, read_only_sql_transaction
    if (sqlState == QLatin1String("0A000"))
        return C::Unsupported;
    if (sqlState == QLatin1String("55P03"))
        return C::Timeout; // lock_not_available (lock_timeout)
    if (sqlState.startsWith(QLatin1String("57P")) || sqlState == QLatin1String("25P03"))
        return C::Lost; // admin shutdown / crash / idle timeouts: the session is gone
    const QStringView cls = QStringView(sqlState).left(2);
    if (cls == u"23")
        return C::Constraint;
    if (cls == u"28")
        return C::Auth;
    if (cls == u"42")
        return C::Syntax;
    if (cls == u"08")
        return C::Connection;
    return C::Other;
}

DbError errorFromResult(const PGresult *result, const QString &sql)
{
    DbError e;
    const auto field = [result](int code) {
        return trimmed(PQresultErrorField(result, code));
    };
    e.sqlState = field(PG_DIAG_SQLSTATE);
    e.message = field(PG_DIAG_MESSAGE_PRIMARY);
    if (e.message.isEmpty())
        e.message = trimmed(PQresultErrorMessage(result));
    e.detail = field(PG_DIAG_MESSAGE_DETAIL);
    e.hint = field(PG_DIAG_MESSAGE_HINT);
    e.position = utf16Position(sql, field(PG_DIAG_STATEMENT_POSITION).toInt());
    e.category = categoryFor(e.sqlState, e.message);
    return e;
}

// libpq reports connection-time failures as text only (no SQLSTATE).
DbError connectionError(PGconn *conn)
{
    DbError e;
    e.message = conn ? trimmed(PQerrorMessage(conn)) : QStringLiteral("Out of memory");
    const bool auth = e.message.contains(QLatin1String("password authentication failed"))
        || e.message.contains(QLatin1String("no password supplied"))
        || e.message.contains(QLatin1String("authentication failed"))
        || e.message.contains(QLatin1String("password is required"));
    e.category = auth ? DbError::Category::Auth : DbError::Category::Connection;
    return e;
}

const char *sslMode(SslOptions::Mode mode)
{
    switch (mode) {
    case SslOptions::Mode::Disable:
        return "disable";
    case SslOptions::Mode::Prefer:
        return "prefer";
    case SslOptions::Mode::Require:
        return "require";
    case SslOptions::Mode::VerifyCa:
        return "verify-ca";
    case SslOptions::Mode::VerifyFull:
        return "verify-full";
    }
    return "prefer";
}

ServerMessage::Severity severityFor(QByteArrayView severity)
{
    if (severity.startsWith("DEBUG"))
        return ServerMessage::Severity::Debug;
    if (severity == QByteArrayView("LOG") || severity == QByteArrayView("INFO"))
        return ServerMessage::Severity::Info;
    if (severity == QByteArrayView("WARNING"))
        return ServerMessage::Severity::Warning;
    return ServerMessage::Severity::Notice;
}

// Command tags whose row count means "rows changed" (the rest report -1).
bool isDmlTag(const char *status)
{
    const QByteArrayView tag(status);
    return tag.startsWith("INSERT") || tag.startsWith("UPDATE") || tag.startsWith("DELETE")
        || tag.startsWith("MERGE") || tag.startsWith("COPY");
}

qint64 affectedFrom(const PGresult *result, bool rowsResult)
{
    const char *tuples = PQcmdTuples(const_cast<PGresult *>(result));
    if (!tuples || !*tuples
        || (rowsResult && !isDmlTag(PQcmdStatus(const_cast<PGresult *>(result)))))
        return -1;
    return QByteArrayView(tuples).toLongLong();
}

class PgConnection;

class PgCursor : public IResultCursor
{
public:
    PgCursor(PgConnection &connection, PgResult first, int previewLimit, bool cancelOnClose);
    ~PgCursor() override { PgCursor::close(); }
    PgCursor(const PgCursor &) = delete;
    PgCursor &operator=(const PgCursor &) = delete;

    QList<ColumnInfo> columns() const override { return m_columns; }
    Result<RowBatch> fetch(qsizetype maxRows) override;
    bool atEnd() const override { return m_done; }
    qint64 affectedRows() const override { return m_affected; }
    void close() override;

    void setAffected(qint64 affected) { m_affected = affected; }

private:
    // Reads the next result from libpq; false at the end (or on error, stored in `error`).
    bool nextChunk(std::optional<DbError> &error);
    // In chunked mode the command tag ("INSERT 0 2") comes with the last chunk, not with the
    // final empty result, so every result is checked.
    void noteCommandTag(const PGresult *result)
    {
        if (const qint64 affected = affectedFrom(result, true); affected >= 0)
            m_affected = affected;
    }
    void finish();

    PgConnection &m_conn;
    PgResult m_result;
    int m_row = 0;
    bool m_final = false; // m_result is the final PGRES_TUPLES_OK
    bool m_done = false;
    bool m_open = true; // still owns the connection's in-flight query
    bool m_cancelOnClose;
    int m_previewLimit;
    qint64 m_affected = -1;
    QList<ColumnInfo> m_columns;
    QList<quint32> m_types;
};

class PgConnection : public IConnection
{
public:
    ~PgConnection() override { PgConnection::close(); }
    PgConnection() = default;
    PgConnection(const PgConnection &) = delete;
    PgConnection &operator=(const PgConnection &) = delete;

    Result<void> open(const ConnectionParams &params) override;
    void close() override;
    bool isOpen() const override { return m_conn != nullptr; }
    bool ping() override;
    ServerInfo serverInfo() const override { return m_info; }
    std::shared_ptr<ICancelHandle> cancelHandle() override
    {
        return std::make_shared<PgCancel>(m_cancel);
    }
    Result<std::unique_ptr<IResultCursor>> execute(const Statement &statement,
                                                   const ExecOptions &options) override;
    Result<void> setAutocommit(bool on) override;
    TxState txState() const override;
    QList<ServerMessage> takeMessages() override { return std::exchange(m_messages, {}); }
    ICatalog &catalog() override { return m_catalog; }

private:
    friend class PgCursor;

    static void noticeReceiver(void *self, const PGresult *result);
    // All types (at open), or only `missing` ones seen in results since (composite row types,
    // types created later).
    void loadTypes(const QSet<quint32> &missing = {});
    Result<void> applyReadOnly(bool readOnly);
    // Reads the remaining results of the current query and drops them.
    void drain();
    DbError lastError(const QString &sql = {}, const PGresult *result = nullptr) const;
    void cursorClosed() { m_busy = false; }

    PGconn *m_conn = nullptr;
    std::shared_ptr<CancelState> m_cancel = std::make_shared<CancelState>();
    TypeRegistry m_types;
    PgCatalog m_catalog{*this, m_types};
    QSet<quint32> m_missingTypes;
    QList<ServerMessage> m_messages;
    ServerInfo m_info;
    PgDialect m_dialect;
    bool m_busy = false; // a cursor is still reading results
    bool m_readOnly = false; // default_transaction_read_only as set by us
    TxState m_txBeforeQuery = TxState::Idle;
};

// --- PgConnection -------------------------------------------------------------------------------

Result<void> PgConnection::open(const ConnectionParams &params)
{
    close();
    m_catalog.reset();

    QList<QByteArray> keys;
    QList<QByteArray> values;
    const auto add = [&](const char *key, const QByteArray &value) {
        if (!value.isEmpty()) {
            keys.append(key);
            values.append(value);
        }
    };
    add("host",
        (params.transport == ConnectionParams::Transport::Socket ? params.socketPath : params.host)
            .toUtf8());
    if (params.port != 0)
        add("port", QByteArray::number(params.port));
    add("user", params.user.toUtf8());
    add("password", params.password.toUtf8());
    add("dbname", params.database.toUtf8());
    add("application_name", params.applicationName.toUtf8());
    add("connect_timeout", QByteArray::number(params.connectTimeout.count()));
    add("client_encoding", "UTF8");
    add("sslmode", sslMode(params.ssl.mode));
    add("sslrootcert", params.ssl.caFile.toUtf8());
    add("sslcert", params.ssl.certFile.toUtf8());
    add("sslkey", params.ssl.keyFile.toUtf8());

    if (!params.driverOptions.isEmpty()) {
        QSet<QByteArray> known;
        if (PQconninfoOption *defaults = PQconndefaults()) {
            for (PQconninfoOption *o = defaults; o->keyword; ++o)
                known.insert(o->keyword);
            PQconninfoFree(defaults);
        }
        for (auto it = params.driverOptions.cbegin(); it != params.driverOptions.cend(); ++it) {
            const QByteArray key = it.key().toUtf8();
            if (!known.contains(key))
                return fail(
                    DbError::Category::Connection,
                    QStringLiteral("Unknown PostgreSQL connection option \"%1\"").arg(it.key()));
            if (const qsizetype i = keys.indexOf(key); i >= 0)
                values[i] = it.value().toString().toUtf8();
            else
                add(key.constData(), it.value().toString().toUtf8());
        }
    }

    std::vector<const char *> keywordPtrs;
    std::vector<const char *> valuePtrs;
    for (qsizetype i = 0; i < keys.size(); ++i) {
        keywordPtrs.push_back(keys.at(i).constData());
        valuePtrs.push_back(values.at(i).constData());
    }
    keywordPtrs.push_back(nullptr);
    valuePtrs.push_back(nullptr);

    PGconn *conn = PQconnectdbParams(keywordPtrs.data(), valuePtrs.data(), /*expand_dbname=*/0);
    if (!conn || PQstatus(conn) != CONNECTION_OK) {
        DbError e = connectionError(conn);
        PQfinish(conn);
        return fail(e);
    }
    m_conn = conn;
    PQsetNoticeReceiver(m_conn, &PgConnection::noticeReceiver, this);

    // The value parsers expect ISO dates and postgres-style intervals (pgvalues.cpp).
    const PgResult setup(
        PQexec(m_conn, "SET DateStyle = 'ISO, YMD'; SET IntervalStyle = 'postgres'"));
    if (PQresultStatus(setup.get()) != PGRES_COMMAND_OK) {
        DbError e = errorFromResult(setup.get(), {});
        close();
        return fail(e);
    }
    loadTypes();
    m_messages.clear();

    m_info.product = QStringLiteral("PostgreSQL");
    m_info.version
        = QString::fromUtf8(PQparameterStatus(m_conn, "server_version")).section(u' ', 0, 0);
    m_info.versionNumber = PQserverVersion(m_conn);

    QMutexLocker lock(&m_cancel->mutex);
    m_cancel->cancel = PQcancelCreate(m_conn);
    return {};
}

void PgConnection::close()
{
    {
        QMutexLocker lock(&m_cancel->mutex);
        if (m_cancel->cancel) {
            PQcancelFinish(m_cancel->cancel);
            m_cancel->cancel = nullptr;
        }
    }
    if (m_conn) {
        PQfinish(m_conn); // cursors are closed by the worker before this
        m_conn = nullptr;
    }
    m_busy = false;
    m_readOnly = false;
}

bool PgConnection::ping()
{
    if (!m_conn || PQstatus(m_conn) != CONNECTION_OK)
        return false;
    if (m_busy)
        return true;
    const PgResult r(PQexec(m_conn, ""));
    return PQresultStatus(r.get()) == PGRES_EMPTY_QUERY;
}

TxState PgConnection::txState() const
{
    if (!m_conn)
        return TxState::Idle;
    switch (PQtransactionStatus(m_conn)) {
    case PQTRANS_INTRANS:
        return TxState::InTransaction;
    case PQTRANS_INERROR:
        return TxState::Failed;
    case PQTRANS_ACTIVE:
        return m_txBeforeQuery; // a result is still streaming
    default:
        return TxState::Idle;
    }
}

Result<void> PgConnection::setAutocommit(bool on)
{
    if (on)
        return {};
    return fail(DbError::Category::Unsupported,
                QStringLiteral("Manual transaction mode arrives with transactions (M2)"));
}

void PgConnection::noticeReceiver(void *self, const PGresult *result)
{
    ServerMessage m;
    m.severity = severityFor(PQresultErrorField(result, PG_DIAG_SEVERITY_NONLOCALIZED));
    m.sqlState = trimmed(PQresultErrorField(result, PG_DIAG_SQLSTATE));
    m.text = trimmed(PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY));
    if (const QString detail = trimmed(PQresultErrorField(result, PG_DIAG_MESSAGE_DETAIL));
        !detail.isEmpty())
        m.text += QStringLiteral("\nDETAIL: ") + detail;
    if (const QString hint = trimmed(PQresultErrorField(result, PG_DIAG_MESSAGE_HINT));
        !hint.isEmpty())
        m.text += QStringLiteral("\nHINT: ") + hint;
    static_cast<PgConnection *>(self)->m_messages.append(m);
}

// Extension types get names, domains the kind of their base type, arrays of user types their
// element. Table row types are only looked up when a result contains one.
void PgConnection::loadTypes(const QSet<quint32> &missing)
{
    const QByteArray columns = "SELECT oid, typname, typtype, typelem, typbasetype, typcategory "
                               "FROM pg_catalog.pg_type ";
    PgResult r;
    if (missing.isEmpty()) {
        const QByteArray sql = columns + "WHERE typtype <> 'c' OR typrelid = 0";
        r.reset(PQexec(m_conn, sql.constData()));
    } else {
        QByteArray list = "{";
        for (const quint32 typeOid : missing)
            list += QByteArray::number(typeOid) + ',';
        list.back() = '}';
        const std::array<const char *, 1> values{list.constData()};
        const QByteArray sql = columns + "WHERE oid = ANY($1::oid[])";
        r.reset(
            PQexecParams(m_conn, sql.constData(), 1, nullptr, values.data(), nullptr, nullptr, 0));
    }
    // Whatever stays unknown is not asked for again.
    const auto markUnresolved = qScopeGuard([&] {
        for (const quint32 typeOid : missing) {
            if (!m_types.find(typeOid))
                m_types.insert(typeOid, TypeInfo{.name = QStringLiteral("oid:%1").arg(typeOid)});
        }
    });
    if (PQresultStatus(r.get()) != PGRES_TUPLES_OK)
        return; // keep the built-ins; values of other types show as text
    struct Domain
    {
        quint32 oid;
        QString name;
        quint32 base;
    };
    QList<Domain> domains;
    for (int row = 0; row < PQntuples(r.get()); ++row) {
        const auto text = [&](int col) {
            return QByteArrayView(PQgetvalue(r.get(), row, col));
        };
        const quint32 typeOid = text(0).toUInt();
        if (typeOid < 16384 && m_types.kind(typeOid) != ValueKind::Unknown)
            continue; // built-in with a dedicated kind
        TypeInfo info;
        info.name = QString::fromUtf8(text(1));
        const char typtype = text(2).at(0);
        const quint32 element = text(3).toUInt();
        const char category = text(5).at(0);
        if (typtype == 'd') {
            domains.append({.oid = typeOid, .name = info.name, .base = text(4).toUInt()});
            continue;
        }
        if (category == 'A' && element != 0) {
            info.kind = ValueKind::Array;
            info.elementOid = element;
        } else if (category == 'S') {
            info.kind = ValueKind::Text; // string types such as citext
        }
        m_types.insert(typeOid, info);
    }
    // Domains take their base type's kind; resolve chains of domains over domains.
    for (int pass = 0; pass < 8 && !domains.isEmpty(); ++pass) {
        QList<Domain> pending;
        for (const Domain &d : domains) {
            if (const TypeInfo *base = m_types.find(d.base)) {
                TypeInfo info = *base;
                info.name = d.name;
                m_types.insert(d.oid, info);
            } else {
                pending.append(d);
            }
        }
        domains = pending;
    }
}

Result<void> PgConnection::applyReadOnly(bool readOnly)
{
    // Only between transactions: inside one, the mode it started with applies, and a failed
    // transaction must still accept ROLLBACK. Re-asserted per statement because RESET ALL or a
    // user SET may have changed it.
    if (PQtransactionStatus(m_conn) != PQTRANS_IDLE || (!readOnly && !m_readOnly))
        return {};
    const PgResult r(PQexec(m_conn,
                            readOnly ? "SET default_transaction_read_only = on"
                                     : "SET default_transaction_read_only = off"));
    if (PQresultStatus(r.get()) != PGRES_COMMAND_OK)
        return fail(lastError({}, r.get()));
    m_readOnly = readOnly;
    return {};
}

DbError PgConnection::lastError(const QString &sql, const PGresult *result) const
{
    DbError e;
    if (result && PQresultStatus(result) == PGRES_FATAL_ERROR)
        e = errorFromResult(result, sql);
    else
        e.message = trimmed(PQerrorMessage(m_conn));
    if (PQstatus(m_conn) == CONNECTION_BAD) {
        e.category = DbError::Category::Lost;
        if (e.message.isEmpty())
            e.message = QStringLiteral("The connection to the server was lost");
    }
    return e;
}

void PgConnection::drain()
{
    while (PGresult *r = PQgetResult(m_conn)) {
        const ExecStatusType status = PQresultStatus(r);
        PQclear(r);
        if (status == PGRES_COPY_IN || status == PGRES_COPY_BOTH) {
            (void)PQputCopyEnd(m_conn, "COPY FROM STDIN is not supported");
        } else if (status == PGRES_COPY_OUT) {
            char *buffer = nullptr;
            while (PQgetCopyData(m_conn, &buffer, 0) > 0)
                PQfreemem(buffer);
        }
        if (PQstatus(m_conn) == CONNECTION_BAD)
            break;
    }
}

Result<std::unique_ptr<IResultCursor>> PgConnection::execute(const Statement &statement,
                                                             const ExecOptions &options)
{
    if (!m_conn)
        return fail(DbError::Category::Connection, QStringLiteral("Not connected"));
    if (PQstatus(m_conn) != CONNECTION_OK)
        return fail(lastError());
    if (m_busy)
        return fail(DbError::Category::Unsupported,
                    QStringLiteral("Another result is still being read on this connection"));
    if (!m_missingTypes.isEmpty())
        loadTypes(std::exchange(m_missingTypes, {}));
    if (Result<void> ro = applyReadOnly(options.readOnly); !ro)
        return fail(ro.error());

    // Parameters in text format with unspecified types: the server infers them from context,
    // like literals. Only bytea is typed, so its hex text is not read as a string.
    const qsizetype count = statement.params.size();
    std::vector<QByteArray> storage(static_cast<size_t>(count));
    std::vector<const char *> values(static_cast<size_t>(count), nullptr);
    std::vector<Oid> types(static_cast<size_t>(count), 0);
    for (qsizetype i = 0; i < count; ++i) {
        const Value &v = statement.params.at(i);
        const auto u = static_cast<size_t>(i);
        if (std::optional<QByteArray> text = toParamText(v)) {
            storage[u] = std::move(*text);
            values[u] = storage[u].constData();
        }
        if (v.kind() == ValueKind::Bytes)
            types[u] = oid::Bytea;
    }

    m_txBeforeQuery = txState();
    const QByteArray sql = statement.sql.toUtf8();
    // The extended protocol runs exactly one statement (decision 143) and binds parameters.
    if (PQsendQueryParams(m_conn, sql.constData(), static_cast<int>(count), types.data(),
                          values.data(), nullptr, nullptr, 0)
        == 0)
        return fail(lastError());
    (void)PQsetChunkedRowsMode(m_conn, chunkRows);

    PgResult first(PQgetResult(m_conn));
    switch (PQresultStatus(first.get())) {
    case PGRES_TUPLES_OK:
    case PGRES_TUPLES_CHUNK: {
        m_busy = true;
        const bool read = m_dialect.classify(statement.sql) == StatementClass::Read;
        return std::unique_ptr<IResultCursor>(
            std::make_unique<PgCursor>(*this, std::move(first), options.previewLimitBytes,
                                       read && m_txBeforeQuery == TxState::Idle));
    }
    case PGRES_COMMAND_OK:
    case PGRES_EMPTY_QUERY: {
        const qint64 affected = affectedFrom(first.get(), false);
        drain();
        auto cursor = std::make_unique<PgCursor>(*this, nullptr, 0, false);
        cursor->setAffected(affected);
        return std::unique_ptr<IResultCursor>(std::move(cursor));
    }
    case PGRES_COPY_IN:
    case PGRES_COPY_OUT:
    case PGRES_COPY_BOTH:
        first.reset();
        drain(); // ends the COPY; its error result is dropped
        return fail(DbError::Category::Unsupported,
                    QStringLiteral("COPY FROM STDIN / TO STDOUT is not supported yet"));
    default: {
        const DbError e = lastError(statement.sql, first.get());
        first.reset();
        drain();
        return fail(e);
    }
    }
}

// --- PgCursor -----------------------------------------------------------------------------------

PgCursor::PgCursor(PgConnection &connection, PgResult first, int previewLimit, bool cancelOnClose)
    : m_conn(connection)
    , m_result(std::move(first))
    , m_cancelOnClose(cancelOnClose)
    , m_previewLimit(previewLimit)
{
    if (!m_result) {
        m_done = true;
        m_open = false;
        return;
    }
    const PGresult *r = m_result.get();
    const int count = PQnfields(r);
    for (int i = 0; i < count; ++i) {
        const quint32 typeOid = PQftype(r, i);
        const int typmod = PQfmod(r, i);
        ColumnInfo c;
        c.name = QString::fromUtf8(PQfname(r, i));
        c.nativeTypeId = typeOid;
        c.kind = m_conn.m_types.kind(typeOid);
        c.nativeType = m_conn.m_types.displayName(typeOid, typmod);
        if (c.nativeType.isEmpty())
            m_conn.m_missingTypes.insert(typeOid); // looked up before the next statement
        const TypeModifiers mods = decodeTypmod(typeOid, typmod);
        c.precision = mods.precision;
        c.scale = mods.scale;
        c.length = mods.length;
        m_columns.append(c);
        m_types.append(typeOid);
    }
    noteCommandTag(r);
    if (PQresultStatus(r) == PGRES_TUPLES_OK) { // the whole (small) result in one go
        m_final = true;
        m_conn.drain();
        if (PQntuples(r) == 0)
            finish();
    }
}

bool PgCursor::nextChunk(std::optional<DbError> &error)
{
    PgResult next(PQgetResult(m_conn.m_conn));
    switch (PQresultStatus(next.get())) {
    case PGRES_TUPLES_CHUNK:
        noteCommandTag(next.get());
        m_result = std::move(next);
        m_row = 0;
        return true;
    case PGRES_TUPLES_OK: // final; empty in chunked mode
        noteCommandTag(next.get());
        m_result = std::move(next);
        m_row = 0;
        m_final = true;
        m_conn.drain();
        return PQntuples(m_result.get()) > 0;
    default:
        if (next) {
            error = m_conn.lastError({}, next.get());
            next.reset();
            m_conn.drain();
        } else if (PQstatus(m_conn.m_conn) == CONNECTION_BAD) {
            error = m_conn.lastError();
        }
        return false;
    }
}

Result<RowBatch> PgCursor::fetch(qsizetype maxRows)
{
    RowBatch batch;
    batch.columnCount = m_columns.size();
    if (m_done || m_columns.isEmpty())
        return batch;
    qsizetype rows = 0;
    while (rows < maxRows) {
        if (m_row >= PQntuples(m_result.get())) {
            std::optional<DbError> error;
            if (m_final || !nextChunk(error)) {
                finish();
                if (error)
                    return fail(*error);
                break;
            }
            continue;
        }
        const PGresult *r = m_result.get();
        for (int c = 0; c < m_columns.size(); ++c) {
            if (PQgetisnull(r, m_row, c)) {
                batch.cells.append(Value());
                continue;
            }
            const QByteArrayView text(PQgetvalue(r, m_row, c), PQgetlength(r, m_row, c));
            batch.cells.append(parseValue(text, m_types.at(c), m_conn.m_types, m_previewLimit));
        }
        ++m_row;
        ++rows;
    }
    if (m_final && m_row >= PQntuples(m_result.get()))
        finish();
    return batch;
}

void PgCursor::finish()
{
    m_done = true;
    m_result.reset();
    if (m_open) {
        m_open = false;
        m_conn.cursorClosed();
    }
}

// Discarding a partly read result: a read outside a transaction is cancelled on the server (the
// rest is never computed); anything else is read to the end, because cancelling would roll back
// a write or abort the user's open transaction.
void PgCursor::close()
{
    if (!m_open)
        return;
    if (!m_final) {
        m_result.reset();
        if (m_cancelOnClose)
            sendCancel(*m_conn.m_cancel);
        m_conn.drain();
    }
    finish();
}

} // namespace

Capabilities PgDriver::capabilities() const
{
    return {Capability::Schemas, Capability::MultipleDatabases, Capability::TransactionalDdl,
            Capability::Returning, Capability::ServerSideCancel};
}

std::unique_ptr<IConnection> PgDriver::createConnection() const
{
    return std::make_unique<PgConnection>();
}

ConnectionFormSchema PgDriver::connectionForm() const
{
    ConnectionFormSchema schema;
    schema.fields = form::serverFields(5432, QStringLiteral("postgres"), true, true,
                                       QStringLiteral("/var/run/postgresql"));
    const QString advanced = QStringLiteral("advanced");
    schema.fields << form::text(QStringLiteral("applicationName"),
                                QStringLiteral("Application name"), advanced, false,
                                QStringLiteral("OwlSql"));
    FormField options
        = form::text(QStringLiteral("option.options"), QStringLiteral("Server options"), advanced);
    options.placeholder = QStringLiteral("-c search_path=app,public");
    schema.fields << options;
    FormField attrs = form::text(QStringLiteral("option.target_session_attrs"),
                                 QStringLiteral("Target session"), advanced);
    attrs.type = FormField::Type::Choice;
    attrs.choices = {{.value = QString(), .label = QStringLiteral("Any")},
                     {.value = QStringLiteral("read-write"), .label = QStringLiteral("Read-write")},
                     {.value = QStringLiteral("read-only"), .label = QStringLiteral("Read-only")},
                     {.value = QStringLiteral("primary"), .label = QStringLiteral("Primary")},
                     {.value = QStringLiteral("standby"), .label = QStringLiteral("Standby")}};
    schema.fields << attrs;
    return schema;
}

// libpq parses both forms: "postgresql://user@host/db?sslmode=require" and
// "host=… dbname=…" (https://www.postgresql.org/docs/current/libpq-connect.html).
Result<ConnectionParams> PgDriver::parseUrl(const QString &url) const
{
    const QString text = url.trimmed();
    static const QRegularExpression keywordForm(QStringLiteral("^[a-z_]+\\s*="));
    const bool uri = text.startsWith(u"postgresql://") || text.startsWith(u"postgres://");
    if (!uri && !keywordForm.match(text).hasMatch())
        return fail(DbError::Category::Unsupported, QStringLiteral("Not a PostgreSQL URL"));

    char *error = nullptr;
    PQconninfoOption *options = PQconninfoParse(text.toUtf8().constData(), &error);
    if (!options) {
        DbError e;
        e.message = QString::fromUtf8(error ? error : "Invalid connection string").trimmed();
        PQfreemem(error);
        if (!uri) // "x=y" that libpq does not know: not ours
            e.category = DbError::Category::Unsupported;
        return fail(e);
    }
    ConnectionParams p;
    p.driverId = id();
    for (PQconninfoOption *o = options; o->keyword; ++o) {
        if (!o->val)
            continue;
        const QString key = QString::fromLatin1(o->keyword);
        const QString value = QString::fromUtf8(o->val);
        if (key == u"host") {
            if (value.startsWith(u'/') || value.startsWith(u'@')) {
                p.transport = ConnectionParams::Transport::Socket;
                p.socketPath = value;
            } else {
                p.host = value;
            }
        } else if (key == u"port") {
            bool ok = false;
            const uint port = value.section(u',', 0, 0).toUInt(&ok);
            if (!ok || port == 0 || port > 65535) {
                PQconninfoFree(options);
                DbError e;
                e.message = QStringLiteral("Invalid port \"%1\"").arg(value);
                return fail(e);
            }
            p.port = static_cast<quint16>(port);
        } else if (key == u"dbname") {
            p.database = value;
        } else if (key == u"user") {
            p.user = value;
        } else if (key == u"password") {
            p.password = value;
        } else if (key == u"sslmode") {
            p.ssl.mode = sslModeFromName(value).value_or(SslOptions::Mode::Prefer);
        } else if (key == u"sslrootcert") {
            p.ssl.caFile = value;
        } else if (key == u"sslcert") {
            p.ssl.certFile = value;
        } else if (key == u"sslkey") {
            p.ssl.keyFile = value;
        } else if (key == u"application_name") {
            p.applicationName = value;
        } else if (key == u"connect_timeout") {
            p.connectTimeout = std::chrono::seconds(qMax(1, value.toInt()));
        } else {
            p.driverOptions.insert(key, value);
        }
    }
    PQconninfoFree(options);
    return p;
}

} // namespace owl::postgres
