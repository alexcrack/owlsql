// SPDX-License-Identifier: GPL-3.0-or-later
#include "sqlitedriver.h"

#include "sqlitecatalog.h"

#include <QMutex>
#include <QMutexLocker>

#include <limits>
#include <sqlite3.h>

namespace owl::sqlite {

namespace {

// The handle shared with the cancel handle: sqlite3_interrupt() is safe from other threads as
// long as the connection is not being closed at the same time.
struct Handle
{
    QMutex mutex;
    sqlite3 *db = nullptr;
};

// sqlite3_error_offset() is a byte offset into the UTF-8 SQL; editors need UTF-16 offsets.
int utf16Offset(const QByteArray &utf8, int byteOffset)
{
    if (byteOffset < 0)
        return -1;
    return static_cast<int>(QString::fromUtf8(utf8.left(byteOffset)).size());
}

DbError makeError(sqlite3 *db, int rc, const QByteArray &sqlUtf8 = {})
{
    DbError e;
    e.nativeCode = rc;
    e.message = db ? QString::fromUtf8(sqlite3_errmsg(db)) : QString::fromUtf8(sqlite3_errstr(rc));
    switch (rc & 0xff) {
    case SQLITE_CONSTRAINT:
        e.category = DbError::Category::Constraint;
        break;
    case SQLITE_INTERRUPT:
        e.category = DbError::Category::Cancelled;
        break;
    case SQLITE_PERM:
    case SQLITE_AUTH:
    case SQLITE_READONLY:
        e.category = DbError::Category::Permission;
        break;
    case SQLITE_CANTOPEN:
    case SQLITE_NOTADB:
        e.category = DbError::Category::Connection;
        break;
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
        e.category = DbError::Category::Timeout;
        break;
    case SQLITE_ERROR:
        if (e.message.contains(QLatin1String("syntax error"))
            || e.message.contains(QLatin1String("incomplete input"))
            || e.message.contains(QLatin1String("unrecognized token")))
            e.category = DbError::Category::Syntax;
        break;
    default:
        break;
    }
    if (db && !sqlUtf8.isEmpty())
        e.position = utf16Offset(sqlUtf8, sqlite3_error_offset(db));
    return e;
}

// Cuts UTF-8 at `limit` bytes without splitting a multi-byte character.
qsizetype utf8Cut(const char *data, qsizetype size, qsizetype limit)
{
    if (size <= limit)
        return size;
    qsizetype cut = limit;
    while (cut > 0 && (static_cast<unsigned char>(data[cut]) & 0xC0) == 0x80)
        --cut;
    return cut;
}

class SqliteCancel : public ICancelHandle
{
public:
    explicit SqliteCancel(std::shared_ptr<Handle> handle)
        : m_handle(std::move(handle))
    { }
    Result<void> cancel() override
    {
        QMutexLocker lock(&m_handle->mutex);
        if (m_handle->db)
            sqlite3_interrupt(m_handle->db);
        return {};
    }

private:
    std::shared_ptr<Handle> m_handle;
};

class SqliteCursor : public IResultCursor
{
public:
    SqliteCursor(sqlite3 *db, sqlite3_stmt *stmt, bool hasRow, qint64 affected,
                 std::optional<Value> lastInsertId, int previewLimit)
        : m_db(db)
        , m_stmt(stmt)
        , m_hasRow(hasRow)
        , m_affected(affected)
        , m_lastInsertId(std::move(lastInsertId))
        , m_previewLimit(previewLimit)
    {
        const int count = stmt ? sqlite3_column_count(stmt) : 0;
        for (int i = 0; i < count; ++i) {
            ColumnInfo c;
            c.name = QString::fromUtf8(sqlite3_column_name(stmt, i));
            c.nativeType = QString::fromUtf8(sqlite3_column_decltype(stmt, i));
            c.kind = kindForDeclaredType(c.nativeType);
            m_columns.append(c);
        }
    }
    ~SqliteCursor() override { SqliteCursor::close(); }
    SqliteCursor(const SqliteCursor &) = delete;
    SqliteCursor &operator=(const SqliteCursor &) = delete;

    QList<ColumnInfo> columns() const override { return m_columns; }
    bool atEnd() const override { return !m_hasRow; }
    qint64 affectedRows() const override { return m_affected; }
    std::optional<Value> lastInsertId() const override { return m_lastInsertId; }

    Result<RowBatch> fetch(qsizetype maxRows) override
    {
        RowBatch batch;
        batch.columnCount = m_columns.size();
        qsizetype rows = 0;
        while (m_hasRow && rows < maxRows) {
            for (int i = 0; i < m_columns.size(); ++i)
                batch.cells.append(cell(i));
            ++rows;
            const int rc = sqlite3_step(m_stmt);
            if (rc == SQLITE_ROW)
                continue;
            m_hasRow = false;
            if (rc != SQLITE_DONE)
                return fail(makeError(m_db, rc));
        }
        return batch;
    }

    void close() override
    {
        if (m_stmt) {
            sqlite3_finalize(m_stmt);
            m_stmt = nullptr;
        }
        m_hasRow = false;
    }

private:
    Value cell(int i) const
    {
        const ValueKind declared = m_columns.at(i).kind;
        switch (sqlite3_column_type(m_stmt, i)) {
        case SQLITE_INTEGER: {
            const qint64 v = sqlite3_column_int64(m_stmt, i);
            if (declared == ValueKind::Bool && (v == 0 || v == 1))
                return Value::fromBool(v == 1);
            return Value::fromInt(v);
        }
        case SQLITE_FLOAT:
            return Value::fromDouble(sqlite3_column_double(m_stmt, i));
        case SQLITE_TEXT: {
            const auto *data = reinterpret_cast<const char *>(sqlite3_column_text(m_stmt, i));
            const qsizetype size = sqlite3_column_bytes(m_stmt, i);
            const qsizetype keep = utf8Cut(data, size, m_previewLimit);
            const QString text = QString::fromUtf8(data, keep);
            Value v = declared == ValueKind::Json ? Value::json(text) : Value::text(text);
            if (keep < size)
                v.setTruncated(size);
            return v;
        }
        case SQLITE_BLOB: {
            const auto *data = static_cast<const char *>(sqlite3_column_blob(m_stmt, i));
            const qsizetype size = sqlite3_column_bytes(m_stmt, i);
            const qsizetype keep = qMin<qsizetype>(size, m_previewLimit);
            Value v = Value::bytes(QByteArray(data, keep));
            if (keep < size)
                v.setTruncated(size);
            return v;
        }
        default:
            return {};
        }
    }

    sqlite3 *m_db;
    sqlite3_stmt *m_stmt;
    bool m_hasRow;
    qint64 m_affected;
    std::optional<Value> m_lastInsertId;
    int m_previewLimit;
    QList<ColumnInfo> m_columns;
};

class SqliteConnection : public IConnection
{
public:
    ~SqliteConnection() override { SqliteConnection::close(); }

    Result<void> open(const ConnectionParams &params) override
    {
        close();
        const QString path
            = params.database.isEmpty() ? QStringLiteral(":memory:") : params.database;
        int flags = params.driverOptions.value(QStringLiteral("readOnly")).toBool()
            ? SQLITE_OPEN_READONLY
            : SQLITE_OPEN_READWRITE;
        if (params.driverOptions.value(QStringLiteral("create")).toBool())
            flags |= SQLITE_OPEN_CREATE;
        if (path.startsWith(QLatin1String("file:")))
            flags |= SQLITE_OPEN_URI;

        sqlite3 *db = nullptr;
        const int rc = sqlite3_open_v2(path.toUtf8().constData(), &db, flags, nullptr);
        if (rc != SQLITE_OK) {
            DbError e = makeError(db, rc);
            e.category = DbError::Category::Connection;
            sqlite3_close_v2(db);
            return fail(e);
        }
        sqlite3_extended_result_codes(db, 1);
        sqlite3_busy_timeout(db, 5000);

        // Fail early on files that are not databases (or are encrypted).
        char *message = nullptr;
        const int check
            = sqlite3_exec(db, "SELECT count(*) FROM sqlite_schema", nullptr, nullptr, &message);
        sqlite3_free(message);
        if (check != SQLITE_OK) {
            DbError e = makeError(db, check);
            e.category = DbError::Category::Connection;
            sqlite3_close_v2(db);
            return fail(e);
        }

        QMutexLocker lock(&m_handle->mutex);
        m_handle->db = db;
        return {};
    }

    void close() override
    {
        QMutexLocker lock(&m_handle->mutex);
        if (m_handle->db) {
            sqlite3_close_v2(m_handle->db); // cursors are closed by the worker before this
            m_handle->db = nullptr;
        }
    }

    bool isOpen() const override { return db() != nullptr; }
    bool ping() override { return isOpen(); }

    ServerInfo serverInfo() const override
    {
        return {.product = QStringLiteral("SQLite"),
                .version = QString::fromLatin1(sqlite3_libversion()),
                .versionNumber = sqlite3_libversion_number()};
    }

    std::shared_ptr<ICancelHandle> cancelHandle() override
    {
        return std::make_shared<SqliteCancel>(m_handle);
    }

    Result<std::unique_ptr<IResultCursor>> execute(const Statement &statement,
                                                   const ExecOptions &options) override
    {
        sqlite3 *conn = db();
        if (!conn)
            return fail(DbError::Category::Connection, QStringLiteral("Not connected"));

        const QByteArray sql = statement.sql.toUtf8();
        sqlite3_stmt *stmt = nullptr;
        const char *tail = nullptr;
        int rc = sqlite3_prepare_v3(conn, sql.constData(), static_cast<int>(sql.size()), 0, &stmt,
                                    &tail);
        if (rc != SQLITE_OK)
            return fail(makeError(conn, rc, sql));

        // One statement per call: the client splits scripts (specs/driver-api.md §5).
        if (tail && *tail) {
            sqlite3_stmt *next = nullptr;
            const int tailRc = sqlite3_prepare_v3(conn, tail, -1, 0, &next, nullptr);
            const bool more = tailRc == SQLITE_OK && next != nullptr;
            sqlite3_finalize(next);
            if (more) {
                sqlite3_finalize(stmt);
                return fail(DbError::Category::Unsupported,
                            QStringLiteral("Only one statement can be executed at a time"));
            }
        }
        if (!stmt) // only whitespace / comments
            return std::unique_ptr<IResultCursor>(
                std::make_unique<SqliteCursor>(conn, nullptr, false, -1, std::nullopt, 0));

        if (options.readOnly && !sqlite3_stmt_readonly(stmt)) {
            sqlite3_finalize(stmt);
            DbError e;
            e.category = DbError::Category::Permission;
            e.message = QStringLiteral("The session is read-only: this statement would write");
            return fail(e);
        }

        if (Result<void> bound = bind(stmt, statement.params); !bound) {
            sqlite3_finalize(stmt);
            return fail(bound.error());
        }

        const qint64 changesBefore = sqlite3_total_changes64(conn);
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
            DbError e = makeError(conn, rc);
            sqlite3_finalize(stmt);
            return fail(e);
        }
        const bool writes = !sqlite3_stmt_readonly(stmt);
        const qint64 affected
            = writes && sqlite3_column_count(stmt) == 0 ? sqlite3_changes64(conn) : -1;
        std::optional<Value> lastId;
        if (writes && sqlite3_total_changes64(conn) > changesBefore)
            lastId = Value::fromInt(sqlite3_last_insert_rowid(conn));
        return std::unique_ptr<IResultCursor>(std::make_unique<SqliteCursor>(
            conn, stmt, rc == SQLITE_ROW, affected, lastId, options.previewLimitBytes));
    }

    Result<void> setAutocommit(bool on) override
    {
        if (on)
            return {};
        return fail(DbError::Category::Unsupported,
                    QStringLiteral("Manual transaction mode arrives with transactions (M2)"));
    }

    TxState txState() const override
    {
        sqlite3 *conn = db();
        return conn && sqlite3_get_autocommit(conn) == 0 ? TxState::InTransaction : TxState::Idle;
    }

    QList<ServerMessage> takeMessages() override { return {}; }
    ICatalog &catalog() override { return m_catalog; }

private:
    sqlite3 *db() const
    {
        QMutexLocker lock(&m_handle->mutex);
        return m_handle->db;
    }

    static Result<void> bind(sqlite3_stmt *stmt, const QList<Value> &params)
    {
        const int expected = sqlite3_bind_parameter_count(stmt);
        if (params.size() != expected) {
            DbError e;
            e.category = DbError::Category::Other;
            e.message = QStringLiteral("The statement has %1 parameter(s), %2 given")
                            .arg(expected)
                            .arg(params.size());
            return fail(e);
        }
        for (int i = 0; i < expected; ++i) {
            const Value &v = params.at(i);
            const int index = i + 1;
            int rc = SQLITE_OK;
            switch (v.kind()) {
            case ValueKind::Null:
                rc = sqlite3_bind_null(stmt, index);
                break;
            case ValueKind::Bool:
                rc = sqlite3_bind_int(stmt, index, v.toBool() ? 1 : 0);
                break;
            case ValueKind::Int:
                rc = sqlite3_bind_int64(stmt, index, v.toInt());
                break;
            case ValueKind::UInt:
                if (v.toUInt() <= static_cast<quint64>(std::numeric_limits<qint64>::max()))
                    rc = sqlite3_bind_int64(stmt, index, static_cast<qint64>(v.toUInt()));
                else
                    rc = bindText(stmt, index, QString::number(v.toUInt()));
                break;
            case ValueKind::Double:
                rc = sqlite3_bind_double(stmt, index, v.toDouble());
                break;
            case ValueKind::Bytes: {
                const QByteArray bytes = v.toBytes();
                rc = sqlite3_bind_blob64(stmt, index, bytes.constData(),
                                         static_cast<sqlite3_uint64>(bytes.size()),
                                         SQLITE_TRANSIENT);
                break;
            }
            case ValueKind::Uuid:
                rc = bindText(stmt, index, v.toUuid().toString(QUuid::WithoutBraces));
                break;
            default:
                rc = bindText(stmt, index, v.toText());
                break;
            }
            if (rc != SQLITE_OK)
                return fail(makeError(sqlite3_db_handle(stmt), rc));
        }
        return {};
    }

    static int bindText(sqlite3_stmt *stmt, int index, const QString &text)
    {
        const QByteArray utf8 = text.toUtf8();
        return sqlite3_bind_text64(stmt, index, utf8.constData(),
                                   static_cast<sqlite3_uint64>(utf8.size()), SQLITE_TRANSIENT,
                                   SQLITE_UTF8);
    }

    std::shared_ptr<Handle> m_handle = std::make_shared<Handle>();
    SqliteCatalog m_catalog{*this};
};

} // namespace

Capabilities SqliteDriver::capabilities() const
{
    return {Capability::Returning, Capability::TransactionalDdl, Capability::ServerSideCancel};
}

std::unique_ptr<IConnection> SqliteDriver::createConnection() const
{
    return std::make_unique<SqliteConnection>();
}

} // namespace owl::sqlite
