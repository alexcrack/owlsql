// SPDX-License-Identifier: GPL-3.0-or-later
#include "mysqldriver.h"

#include "mysqlcatalog.h"
#include "mysqlvalues.h"

#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThreadPool>

#include <mysql.h>
#include <mysqld_error.h>
#include <vector>

namespace owl::mysql {

namespace {

constexpr unsigned int binaryCharset = 63;

struct MysqlDeleter
{
    void operator()(MYSQL *m) const { mysql_close(m); }
};
using MysqlPtr = std::unique_ptr<MYSQL, MysqlDeleter>;

// --- Errors -------------------------------------------------------------------------------------

DbError::Category categoryFor(unsigned int code, QByteArrayView sqlState)
{
    using C = DbError::Category;
    switch (code) {
    case ER_PARSE_ERROR:
    case ER_EMPTY_QUERY:
        return C::Syntax;
    case ER_ACCESS_DENIED_ERROR:
    case ER_DBACCESS_DENIED_ERROR:
    case 1698: // ER_ACCESS_DENIED_NO_PASSWORD_ERROR
    case 1251: // ER_NOT_SUPPORTED_AUTH_MODE
    case 2059: // CR_AUTH_PLUGIN_CANNOT_LOAD
        return C::Auth;
    case ER_TABLEACCESS_DENIED_ERROR:
    case ER_COLUMNACCESS_DENIED_ERROR:
    case ER_SPECIFIC_ACCESS_DENIED_ERROR:
    case ER_PROCACCESS_DENIED_ERROR:
    case ER_OPTION_PREVENTS_STATEMENT:
    case ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION:
        return C::Permission;
    case ER_QUERY_INTERRUPTED:
        return C::Cancelled;
    case ER_LOCK_WAIT_TIMEOUT:
    case 3024: // ER_QUERY_TIMEOUT (max_execution_time)
    case 1969: // ER_STATEMENT_TIMEOUT (MariaDB max_statement_time)
        return C::Timeout;
    case ER_UNSUPPORTED_PS:
    case ER_NOT_SUPPORTED_YET:
        return C::Unsupported;
    case 2006: // CR_SERVER_GONE_ERROR
    case 2013: // CR_SERVER_LOST
    case 2055: // CR_SERVER_LOST_EXTENDED
    case ER_SERVER_SHUTDOWN:
    case 1927: // ER_CONNECTION_KILLED (MariaDB)
    case 4031: // ER_CLIENT_INTERACTION_TIMEOUT (MySQL 8)
        return C::Lost;
    case 2002: // CR_CONNECTION_ERROR
    case 2003: // CR_CONN_HOST_ERROR
    case 2005: // CR_UNKNOWN_HOST
    case 2026: // CR_SSL_CONNECTION_ERROR
    case ER_BAD_DB_ERROR:
        return C::Connection;
    default:
        break;
    }
    if (sqlState.startsWith("23"))
        return C::Constraint;
    if (sqlState.startsWith("42"))
        return C::Syntax; // unknown table / column …
    if (sqlState.startsWith("28"))
        return C::Auth;
    if (sqlState.startsWith("08"))
        return C::Connection;
    return C::Other;
}

// Syntax errors name the rest of the statement: "… near 'WHERE' at line 1". The position is where
// that text starts on that line, as a UTF-16 offset.
int errorPosition(const QString &message, const QString &sql)
{
    static const QRegularExpression nearRe(QStringLiteral("near '(.*)' at line (\\d+)$"),
                                           QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch m = nearRe.match(message);
    if (!m.hasMatch() || sql.isEmpty())
        return -1;
    const QString snippet = m.captured(1);
    qsizetype lineStart = 0;
    for (int line = m.captured(2).toInt(); line > 1 && lineStart >= 0; --line) {
        lineStart = sql.indexOf(u'\n', lineStart);
        if (lineStart >= 0)
            ++lineStart;
    }
    if (lineStart < 0)
        return -1;
    if (snippet.isEmpty())
        return static_cast<int>(sql.size()); // "near '' ": unexpected end of input
    const qsizetype at = sql.indexOf(snippet, lineStart);
    return at < 0 ? -1 : static_cast<int>(at);
}

DbError makeError(unsigned int code, const char *sqlState, const char *message,
                  const QString &sql = {})
{
    DbError e;
    e.nativeCode = static_cast<int>(code);
    e.sqlState = QString::fromLatin1(sqlState);
    if (e.sqlState == QLatin1String("HY000") || e.sqlState == QLatin1String("00000"))
        e.sqlState.clear(); // the generic state says nothing
    e.message = QString::fromUtf8(message);
    e.category = categoryFor(code, QByteArrayView(sqlState));
    e.position = errorPosition(e.message, sql);
    return e;
}

DbError connectionError(MYSQL *m, const QString &sql = {})
{
    return makeError(mysql_errno(m), mysql_sqlstate(m), mysql_error(m), sql);
}

DbError statementError(MYSQL_STMT *s, const QString &sql = {})
{
    return makeError(mysql_stmt_errno(s), mysql_stmt_sqlstate(s), mysql_stmt_error(s), sql);
}

// --- Connecting ---------------------------------------------------------------------------------

Result<MysqlPtr> connectHandle(const ConnectionParams &p)
{
    MysqlPtr m(mysql_init(nullptr));
    if (!m)
        return fail(DbError::Category::Other, QStringLiteral("Out of memory"));

    const auto timeout = static_cast<unsigned int>(p.connectTimeout.count());
    mysql_optionsv(m.get(), MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    mysql_optionsv(m.get(), MYSQL_SET_CHARSET_NAME, "utf8mb4");
    const my_bool off = 0;
    const my_bool on = 1;
    mysql_optionsv(m.get(), MYSQL_OPT_LOCAL_INFILE, &off); // LOAD DATA LOCAL: never from a client
    mysql_optionsv(m.get(), MYSQL_OPT_RECONNECT, &off); // reconnects are the worker's job (§7)
    const QByteArray program = p.applicationName.toUtf8();
    mysql_optionsv(m.get(), MYSQL_OPT_CONNECT_ATTR_ADD, "program_name", program.constData());

    // Connector/C 3.4 verifies certificates by default; MySQL servers mostly have self-signed
    // ones, so only the Verify modes verify (decision 157).
    using Mode = SslOptions::Mode;
    const bool verify = p.ssl.mode == Mode::VerifyCa || p.ssl.mode == Mode::VerifyFull;
    mysql_optionsv(m.get(), MYSQL_OPT_SSL_ENFORCE, p.ssl.mode == Mode::Disable ? &off : &on);
    mysql_optionsv(m.get(), MYSQL_OPT_SSL_VERIFY_SERVER_CERT, verify ? &on : &off);
    const QByteArray ca = p.ssl.caFile.toUtf8();
    const QByteArray cert = p.ssl.certFile.toUtf8();
    const QByteArray key = p.ssl.keyFile.toUtf8();
    if (!ca.isEmpty())
        mysql_optionsv(m.get(), MYSQL_OPT_SSL_CA, ca.constData());
    if (!cert.isEmpty())
        mysql_optionsv(m.get(), MYSQL_OPT_SSL_CERT, cert.constData());
    if (!key.isEmpty())
        mysql_optionsv(m.get(), MYSQL_OPT_SSL_KEY, key.constData());
    const QByteArray init
        = p.driverOptions.value(QStringLiteral("initCommand")).toString().toUtf8();
    if (!init.isEmpty())
        mysql_optionsv(m.get(), MYSQL_INIT_COMMAND, init.constData());

    const bool socket = p.transport == ConnectionParams::Transport::Socket;
    const QByteArray host = p.host.toUtf8();
    const QByteArray user = p.user.toUtf8();
    const QByteArray password = p.password.toUtf8();
    const QByteArray database = p.database.toUtf8();
    const QByteArray socketPath = p.socketPath.toUtf8();
    if (!mysql_real_connect(m.get(), socket ? "localhost" : host.constData(), user.constData(),
                            password.constData(),
                            database.isEmpty() ? nullptr : database.constData(), p.port,
                            socket ? socketPath.constData() : nullptr, CLIENT_MULTI_RESULTS)) {
        DbError e = connectionError(m.get());
        if (e.category == DbError::Category::Other || e.category == DbError::Category::Lost)
            e.category = DbError::Category::Connection;
        return fail(e);
    }
    if (p.ssl.mode == Mode::Require && !mysql_get_ssl_cipher(m.get()))
        return fail(DbError::Category::Connection,
                    QStringLiteral("TLS is required, but the server does not support it"));
    return m;
}

// --- Cancel -------------------------------------------------------------------------------------

// What a side connection needs to send KILL QUERY for the session (decision 156).
struct CancelState
{
    QMutex mutex;
    ConnectionParams params;
    unsigned long threadId = 0; // 0: not connected
};

void sendKill(CancelState &state)
{
    ConnectionParams params;
    unsigned long threadId = 0;
    {
        QMutexLocker lock(&state.mutex);
        params = state.params;
        threadId = state.threadId;
    }
    if (threadId == 0)
        return;
    params.database.clear();
    params.driverOptions.remove(QStringLiteral("initCommand"));
    params.connectTimeout = std::chrono::seconds(5);
    Result<MysqlPtr> side = connectHandle(params);
    if (!side)
        return; // not actionable: the statement just runs on
    const QByteArray kill = "KILL QUERY " + QByteArray::number(static_cast<qulonglong>(threadId));
    (void)mysql_real_query(side->get(), kill.constData(), static_cast<unsigned long>(kill.size()));
}

class MySqlCancel : public ICancelHandle
{
public:
    explicit MySqlCancel(std::shared_ptr<CancelState> state)
        : m_state(std::move(state))
    { }

    Result<void> cancel() override
    {
        QThreadPool::globalInstance()->start([state = m_state] { sendKill(*state); });
        return {};
    }

private:
    std::shared_ptr<CancelState> m_state;
};

// --- Columns ------------------------------------------------------------------------------------

unsigned int charMaxLen(unsigned int charsetnr)
{
    if (const MARIADB_CHARSET_INFO *cs = mariadb_get_charset_by_nr(charsetnr);
        cs && cs->char_maxlen)
        return cs->char_maxlen;
    return 1;
}

QString formatName(const MYSQL_FIELD *f)
{
    MARIADB_CONST_STRING attr{};
    if (mariadb_field_attr(&attr, f, MARIADB_FIELD_ATTR_FORMAT_NAME) == 0 && attr.length > 0)
        return QString::fromUtf8(attr.str, static_cast<qsizetype>(attr.length));
    return {};
}

ColumnInfo columnFor(const MYSQL_FIELD *f)
{
    ColumnInfo c;
    c.name = QString::fromUtf8(f->name, f->name_length);
    c.schema = QString::fromUtf8(f->db, f->db_length);
    c.table = QString::fromUtf8(f->org_table, f->org_table_length);
    c.tableColumn = QString::fromUtf8(f->org_name, f->org_name_length);
    c.isPrimaryKey = (f->flags & PRI_KEY_FLAG) != 0;
    c.nullable = (f->flags & NOT_NULL_FLAG) == 0;
    c.nativeTypeId = f->type;

    const bool isUnsigned = (f->flags & UNSIGNED_FLAG) != 0;
    const bool binary = f->charsetnr == binaryCharset;
    const QString unsignedSuffix = isUnsigned ? QStringLiteral(" unsigned") : QString();
    const auto chars = [f] {
        return static_cast<int>(f->length / charMaxLen(f->charsetnr));
    };
    const auto withPrecision = [f](const char *name) {
        return f->decimals > 0 && f->decimals <= 6
            ? QStringLiteral("%1(%2)").arg(QLatin1String(name)).arg(f->decimals)
            : QString::fromLatin1(name);
    };

    switch (f->type) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG: {
        const char *name = f->type == MYSQL_TYPE_TINY ? "tinyint"
            : f->type == MYSQL_TYPE_SHORT             ? "smallint"
            : f->type == MYSQL_TYPE_INT24             ? "mediumint"
            : f->type == MYSQL_TYPE_LONG              ? "int"
                                                      : "bigint";
        c.nativeType = QString::fromLatin1(name) + unsignedSuffix;
        c.kind = f->type == MYSQL_TYPE_LONGLONG && isUnsigned ? ValueKind::UInt : ValueKind::Int;
        break;
    }
    case MYSQL_TYPE_YEAR:
        c.nativeType = QStringLiteral("year");
        c.kind = ValueKind::Int;
        break;
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
        c.nativeType
            = (f->type == MYSQL_TYPE_FLOAT ? QStringLiteral("float") : QStringLiteral("double"))
            + unsignedSuffix;
        c.kind = ValueKind::Double;
        break;
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
        c.scale = static_cast<int>(f->decimals);
        c.precision = static_cast<int>(f->length) - (c.scale > 0 ? 1 : 0) - (isUnsigned ? 0 : 1);
        c.nativeType
            = QStringLiteral("decimal(%1,%2)").arg(c.precision).arg(c.scale) + unsignedSuffix;
        c.kind = ValueKind::Decimal;
        break;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
        c.nativeType = QStringLiteral("date");
        c.kind = ValueKind::Date;
        break;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
        c.nativeType = withPrecision("datetime");
        c.precision = static_cast<int>(f->decimals);
        c.kind = ValueKind::Timestamp;
        break;
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
        c.nativeType = withPrecision("timestamp");
        c.precision = static_cast<int>(f->decimals);
        c.kind = ValueKind::Timestamp; // shown in the session time zone, without an offset
        break;
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
        c.nativeType = withPrecision("time");
        c.precision = static_cast<int>(f->decimals);
        c.kind = ValueKind::Time;
        break;
    case MYSQL_TYPE_JSON:
        c.nativeType = QStringLiteral("json");
        c.kind = ValueKind::Json;
        break;
    case MYSQL_TYPE_BIT:
        c.nativeType = QStringLiteral("bit(%1)").arg(f->length);
        c.length = static_cast<int>(f->length);
        c.kind = ValueKind::Bytes;
        break;
    case MYSQL_TYPE_GEOMETRY:
        c.nativeType = QStringLiteral("geometry");
        c.kind = ValueKind::Bytes;
        break;
    case MYSQL_TYPE_NULL:
        c.nativeType = QStringLiteral("null");
        c.kind = ValueKind::Text;
        break;
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_SET:
        c.nativeType = f->type == MYSQL_TYPE_ENUM ? QStringLiteral("enum") : QStringLiteral("set");
        c.kind = ValueKind::Text;
        break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB: {
        const unsigned long bytes = binary ? f->length : f->length / charMaxLen(f->charsetnr);
        const char *size = bytes <= 255 ? "tiny"
            : bytes <= 65535            ? ""
            : bytes <= 16777215         ? "medium"
                                        : "long";
        c.nativeType
            = QLatin1String(size) + (binary ? QStringLiteral("blob") : QStringLiteral("text"));
        c.kind = binary ? ValueKind::Bytes : ValueKind::Text;
        break;
    }
    default: // VARCHAR, VAR_STRING, STRING (CHAR / BINARY)
        if (f->flags & ENUM_FLAG) {
            c.nativeType = QStringLiteral("enum");
            c.kind = ValueKind::Text;
        } else if (f->flags & SET_FLAG) {
            c.nativeType = QStringLiteral("set");
            c.kind = ValueKind::Text;
        } else {
            const bool fixed = f->type == MYSQL_TYPE_STRING;
            c.length = binary ? static_cast<int>(f->length) : chars();
            c.nativeType
                = QStringLiteral("%1(%2)")
                      .arg(binary ? (fixed ? QStringLiteral("binary") : QStringLiteral("varbinary"))
                                  : (fixed ? QStringLiteral("char") : QStringLiteral("varchar")))
                      .arg(c.length);
            c.kind = binary ? ValueKind::Bytes : ValueKind::Text;
        }
        break;
    }
    // MariaDB extended metadata: JSON columns are longtext with format "json"; uuid, inet6, …
    if (const QString format = formatName(f); !format.isEmpty()) {
        c.nativeType = format;
        if (format == QLatin1String("json"))
            c.kind = ValueKind::Json;
    }
    return c;
}

class MySqlConnection;

// One cursor for both protocols: plain statements use the text protocol (mysql_use_result);
// statements with parameters are server-side prepared statements (decision 155). Rows are read
// unbuffered: pausing simply stops reading.
class MySqlCursor : public IResultCursor
{
public:
    MySqlCursor(MySqlConnection &connection, MYSQL_STMT *stmt, int previewLimit, bool cancelOnClose,
                QString sql);
    ~MySqlCursor() override { MySqlCursor::close(); }
    MySqlCursor(const MySqlCursor &) = delete;
    MySqlCursor &operator=(const MySqlCursor &) = delete;

    // Sets up the first result; on failure the cursor is closed.
    Result<void> start();

    QList<ColumnInfo> columns() const override { return m_columns; }
    Result<RowBatch> fetch(qsizetype maxRows) override;
    bool atEnd() const override { return m_rowsDone; }
    qint64 affectedRows() const override { return m_affected; }
    std::optional<Value> lastInsertId() const override { return m_insertId; }
    bool nextResult() override;
    void close() override;

private:
    struct Cell
    {
        QByteArray data;
        unsigned long length = 0;
        my_bool isNull = 0;
        my_bool error = 0;
        qint64 integer = 0;
        double real = 0;
    };

    Result<void> openResult(); // columns of the current result, or its affected rows
    void releaseResult(); // drops the rest of the current result
    bool advance(); // to the next result set; false when there is none
    void finish(); // all results consumed: the connection is free again
    Value cellValue(qsizetype column, const char *data, unsigned long length) const;

    MySqlConnection &m_conn;
    MYSQL *m_mysql;
    MYSQL_STMT *m_stmt; // null: text protocol
    MYSQL_RES *m_res = nullptr;
    std::vector<MYSQL_BIND> m_binds;
    std::vector<Cell> m_cells;
    QList<ColumnInfo> m_columns;
    int m_previewLimit;
    bool m_cancelOnClose;
    QString m_sql;
    bool m_rowsDone = true;
    bool m_open = true;
    qint64 m_affected = -1;
    std::optional<Value> m_insertId;
};

class MySqlConnection : public IConnection
{
public:
    MySqlConnection() = default;
    ~MySqlConnection() override { MySqlConnection::close(); }
    MySqlConnection(const MySqlConnection &) = delete;
    MySqlConnection &operator=(const MySqlConnection &) = delete;

    Result<void> open(const ConnectionParams &params) override;
    void close() override;
    bool isOpen() const override { return m_mysql != nullptr; }
    bool ping() override;
    ServerInfo serverInfo() const override { return m_info; }
    std::shared_ptr<ICancelHandle> cancelHandle() override
    {
        return std::make_shared<MySqlCancel>(m_cancel);
    }
    Result<std::unique_ptr<IResultCursor>> execute(const Statement &statement,
                                                   const ExecOptions &options) override;
    Result<void> setAutocommit(bool on) override;
    TxState txState() const override;
    QList<ServerMessage> takeMessages() override { return std::exchange(m_messages, {}); }
    ICatalog &catalog() override { return m_catalog; }

private:
    friend class MySqlCursor;

    bool inTransaction() const;
    bool isEmptyStatement(const QString &sql) const;
    Result<void> applyReadOnly(bool readOnly);
    Result<void> run(const char *sql); // internal statement without results
    void collectWarnings(); // SHOW WARNINGS after a statement that produced some
    void cursorFinished()
    {
        m_busy = false;
        collectWarnings();
    }
    Result<std::unique_ptr<IResultCursor>> prepared(const Statement &statement,
                                                    const ExecOptions &options, bool cancelOnClose);

    MYSQL *m_mysql = nullptr;
    std::shared_ptr<CancelState> m_cancel = std::make_shared<CancelState>();
    ServerInfo m_info;
    QList<ServerMessage> m_messages;
    MySqlDialect m_dialect;
    MySqlCatalog m_catalog{*this};
    bool m_busy = false;
    bool m_readOnly = false;
};

// --- MySqlConnection ----------------------------------------------------------------------------

Result<void> MySqlConnection::open(const ConnectionParams &params)
{
    close();
    Result<MysqlPtr> handle = connectHandle(params);
    if (!handle)
        return fail(handle.error());
    m_mysql = handle->release();

    const QString version = QString::fromUtf8(mysql_get_server_info(m_mysql));
    m_info.product = version.contains(QLatin1String("MariaDB"), Qt::CaseInsensitive)
        ? QStringLiteral("MariaDB")
        : QStringLiteral("MySQL");
    m_info.version = version.section(u'-', 0, 0);
    m_info.versionNumber = static_cast<int>(mysql_get_server_version(m_mysql));
    m_messages.clear();

    QMutexLocker lock(&m_cancel->mutex);
    m_cancel->params = params;
    m_cancel->threadId = mysql_thread_id(m_mysql);
    return {};
}

void MySqlConnection::close()
{
    {
        QMutexLocker lock(&m_cancel->mutex);
        m_cancel->threadId = 0;
        m_cancel->params = {};
    }
    if (m_mysql) {
        mysql_close(m_mysql); // cursors are closed by the worker before this
        m_mysql = nullptr;
    }
    m_busy = false;
    m_readOnly = false;
}

bool MySqlConnection::ping()
{
    if (!m_mysql)
        return false;
    if (m_busy)
        return true; // reading a result: the connection is evidently alive
    return mysql_ping(m_mysql) == 0;
}

bool MySqlConnection::inTransaction() const
{
    unsigned int status = 0;
    if (!m_mysql || mariadb_get_infov(m_mysql, MARIADB_CONNECTION_SERVER_STATUS, &status) != 0)
        return false;
    return (status & SERVER_STATUS_IN_TRANS) != 0;
}

// MySQL has no failed-transaction state: after an error the transaction simply stays open.
TxState MySqlConnection::txState() const
{
    return inTransaction() ? TxState::InTransaction : TxState::Idle;
}

Result<void> MySqlConnection::setAutocommit(bool on)
{
    if (on)
        return {};
    return fail(DbError::Category::Unsupported,
                QStringLiteral("Manual transaction mode arrives with transactions (M2)"));
}

Result<void> MySqlConnection::run(const char *sql)
{
    if (mysql_real_query(m_mysql, sql, static_cast<unsigned long>(qstrlen(sql))) != 0)
        return fail(connectionError(m_mysql));
    while (mysql_more_results(m_mysql) && mysql_next_result(m_mysql) == 0) { }
    return {};
}

// SET SESSION TRANSACTION READ ONLY applies from the next transaction on; inside an open one its
// own mode applies (decision 157). Re-asserted per statement, like PostgreSQL.
Result<void> MySqlConnection::applyReadOnly(bool readOnly)
{
    if (inTransaction() || (!readOnly && !m_readOnly))
        return {};
    if (Result<void> r = run(readOnly ? "SET SESSION TRANSACTION READ ONLY"
                                      : "SET SESSION TRANSACTION READ WRITE");
        !r)
        return r;
    m_readOnly = readOnly;
    return {};
}

void MySqlConnection::collectWarnings()
{
    if (!m_mysql || mysql_warning_count(m_mysql) == 0)
        return;
    static constexpr QByteArrayView sql("SHOW WARNINGS");
    if (mysql_real_query(m_mysql, sql.data(), static_cast<unsigned long>(sql.size())) != 0)
        return;
    MYSQL_RES *res = mysql_store_result(m_mysql);
    if (!res)
        return;
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        ServerMessage m;
        const QByteArrayView level(row[0] ? row[0] : "");
        m.severity = level == QByteArrayView("Note") ? ServerMessage::Severity::Notice
                                                     : ServerMessage::Severity::Warning;
        m.text = QString::fromUtf8(row[2] ? row[2] : "");
        if (row[1])
            m.text += QStringLiteral(" (%1)").arg(QString::fromLatin1(row[1]));
        m_messages.append(m);
    }
    mysql_free_result(res);
}

// Comment-only statements are not sent (the server would answer "Query was empty"), except
// executable comments: /*! … */ and /*M! … */ run on the server.
bool MySqlConnection::isEmptyStatement(const QString &sql) const
{
    for (const SqlToken &t : m_dialect.tokenize(sql)) {
        if (!t.isTrivia())
            return false;
        const QStringView text = t.text(sql);
        if (t.type == SqlToken::Type::BlockComment
            && (text.startsWith(u"/*!") || text.startsWith(u"/*M!")))
            return false;
    }
    return true;
}

Result<std::unique_ptr<IResultCursor>> MySqlConnection::execute(const Statement &statement,
                                                                const ExecOptions &options)
{
    if (!m_mysql)
        return fail(DbError::Category::Connection, QStringLiteral("Not connected"));
    if (m_busy)
        return fail(DbError::Category::Unsupported,
                    QStringLiteral("Another result is still being read on this connection"));
    if (isEmptyStatement(statement.sql))
        return std::unique_ptr<IResultCursor>(
            std::make_unique<MySqlCursor>(*this, nullptr, 0, false, QString()));
    if (Result<void> ro = applyReadOnly(options.readOnly); !ro)
        return fail(ro.error());

    const bool cancelOnClose
        = !inTransaction() && m_dialect.classify(statement.sql) == StatementClass::Read;
    if (!statement.params.isEmpty())
        return prepared(statement, options, cancelOnClose);

    const QByteArray sql = statement.sql.toUtf8();
    if (mysql_real_query(m_mysql, sql.constData(), static_cast<unsigned long>(sql.size())) != 0)
        return fail(connectionError(m_mysql, statement.sql));
    m_busy = true;
    auto cursor = std::make_unique<MySqlCursor>(*this, nullptr, options.previewLimitBytes,
                                                cancelOnClose, statement.sql);
    if (Result<void> started = cursor->start(); !started)
        return fail(started.error());
    return std::unique_ptr<IResultCursor>(std::move(cursor));
}

Result<std::unique_ptr<IResultCursor>> MySqlConnection::prepared(const Statement &statement,
                                                                 const ExecOptions &options,
                                                                 bool cancelOnClose)
{
    MYSQL_STMT *stmt = mysql_stmt_init(m_mysql);
    if (!stmt)
        return fail(connectionError(m_mysql));
    const auto discard = [stmt](DbError e) {
        mysql_stmt_close(stmt);
        return fail(std::move(e));
    };
    const QByteArray sql = statement.sql.toUtf8();
    if (mysql_stmt_prepare(stmt, sql.constData(), static_cast<unsigned long>(sql.size())) != 0)
        return discard(statementError(stmt, statement.sql));

    const qsizetype count = statement.params.size();
    if (mysql_stmt_param_count(stmt) != static_cast<unsigned long>(count)) {
        DbError e;
        e.message = QStringLiteral("The statement has %1 parameter(s), %2 given")
                        .arg(mysql_stmt_param_count(stmt))
                        .arg(count);
        return discard(e);
    }

    // Native binds for numbers and bytes, text for everything else (the server converts).
    std::vector<MYSQL_BIND> binds(static_cast<size_t>(count));
    std::vector<QByteArray> text(static_cast<size_t>(count));
    std::vector<qint64> integers(static_cast<size_t>(count));
    std::vector<double> reals(static_cast<size_t>(count));
    std::vector<unsigned long> lengths(static_cast<size_t>(count));
    for (qsizetype i = 0; i < count; ++i) {
        const auto u = static_cast<size_t>(i);
        const Value &v = statement.params.at(i);
        MYSQL_BIND &b = binds[u];
        b = MYSQL_BIND{};
        switch (v.kind()) {
        case ValueKind::Null:
            b.buffer_type = MYSQL_TYPE_NULL;
            break;
        case ValueKind::Bool:
        case ValueKind::Int:
        case ValueKind::UInt:
            integers[u] = v.kind() == ValueKind::Bool ? (v.toBool() ? 1 : 0)
                : v.kind() == ValueKind::UInt         ? static_cast<qint64>(v.toUInt())
                                                      : v.toInt();
            b.buffer_type = MYSQL_TYPE_LONGLONG;
            b.buffer = &integers[u];
            b.is_unsigned = v.kind() == ValueKind::UInt ? 1 : 0;
            break;
        case ValueKind::Double:
            reals[u] = v.toDouble();
            b.buffer_type = MYSQL_TYPE_DOUBLE;
            b.buffer = &reals[u];
            break;
        default:
            text[u] = toParamText(v).value_or(QByteArray());
            lengths[u] = static_cast<unsigned long>(text[u].size());
            b.buffer_type = v.kind() == ValueKind::Bytes ? MYSQL_TYPE_BLOB : MYSQL_TYPE_STRING;
            b.buffer = text[u].data();
            b.buffer_length = lengths[u];
            b.length = &lengths[u];
            break;
        }
    }
    if (count > 0 && mysql_stmt_bind_param(stmt, binds.data()) != 0)
        return discard(statementError(stmt));
    if (mysql_stmt_execute(stmt) != 0)
        return discard(statementError(stmt, statement.sql));

    m_busy = true;
    auto cursor = std::make_unique<MySqlCursor>(*this, stmt, options.previewLimitBytes,
                                                cancelOnClose, statement.sql);
    if (Result<void> started = cursor->start(); !started)
        return fail(started.error());
    return std::unique_ptr<IResultCursor>(std::move(cursor));
}

// --- MySqlCursor --------------------------------------------------------------------------------

MySqlCursor::MySqlCursor(MySqlConnection &connection, MYSQL_STMT *stmt, int previewLimit,
                         bool cancelOnClose, QString sql)
    : m_conn(connection)
    , m_mysql(connection.m_mysql)
    , m_stmt(stmt)
    , m_previewLimit(previewLimit)
    , m_cancelOnClose(cancelOnClose)
    , m_sql(std::move(sql))
{
    if (m_sql.isEmpty() && !m_stmt)
        m_open = false; // an empty statement: nothing was sent
}

Result<void> MySqlCursor::start()
{
    Result<void> opened = openResult();
    if (!opened) {
        close();
        return opened;
    }
    // A CALL's own status comes after its result sets; a statement without rows ends here.
    if (m_columns.isEmpty() && !advance())
        finish();
    return {};
}

Result<void> MySqlCursor::openResult()
{
    m_columns.clear();
    m_binds.clear();
    m_cells.clear();
    m_rowsDone = true;
    const unsigned int fieldCount
        = m_stmt ? mysql_stmt_field_count(m_stmt) : mysql_field_count(m_mysql);
    if (fieldCount == 0) {
        const my_ulonglong affected
            = m_stmt ? mysql_stmt_affected_rows(m_stmt) : mysql_affected_rows(m_mysql);
        m_affected = affected == ~my_ulonglong{0} ? -1 : static_cast<qint64>(affected);
        const my_ulonglong id = m_stmt ? mysql_stmt_insert_id(m_stmt) : mysql_insert_id(m_mysql);
        if (id != 0)
            m_insertId = Value::fromUInt(id);
        return {};
    }
    m_affected = -1;
    m_res = m_stmt ? mysql_stmt_result_metadata(m_stmt) : mysql_use_result(m_mysql);
    if (!m_res)
        return fail(m_stmt ? statementError(m_stmt) : connectionError(m_mysql));
    const MYSQL_FIELD *fields = mysql_fetch_fields(m_res);
    for (unsigned int i = 0; i < fieldCount; ++i)
        m_columns.append(columnFor(&fields[i]));
    m_rowsDone = false;

    if (m_stmt) {
        m_binds.assign(fieldCount, MYSQL_BIND{});
        m_cells.resize(fieldCount);
        for (unsigned int i = 0; i < fieldCount; ++i) {
            MYSQL_BIND &b = m_binds[i];
            Cell &cell = m_cells[i];
            const ValueKind kind = m_columns.at(i).kind;
            if (kind == ValueKind::Int || kind == ValueKind::UInt) {
                b.buffer_type = MYSQL_TYPE_LONGLONG;
                b.buffer = &cell.integer;
                b.is_unsigned = kind == ValueKind::UInt ? 1 : 0;
            } else if (kind == ValueKind::Double) {
                b.buffer_type = MYSQL_TYPE_DOUBLE;
                b.buffer = &cell.real;
            } else {
                // A few bytes over the preview limit, so truncation is detected by the parser
                // without cutting a character.
                cell.data.resize(qMax(m_previewLimit, 64) + 4);
                b.buffer_type = MYSQL_TYPE_STRING;
                b.buffer = cell.data.data();
                b.buffer_length = static_cast<unsigned long>(cell.data.size());
            }
            b.length = &cell.length;
            b.is_null = &cell.isNull;
            b.error = &cell.error;
        }
        if (mysql_stmt_bind_result(m_stmt, m_binds.data()) != 0)
            return fail(statementError(m_stmt));
    }
    return {};
}

Value MySqlCursor::cellValue(qsizetype column, const char *data, unsigned long length) const
{
    return parseValue(QByteArrayView(data, static_cast<qsizetype>(length)),
                      m_columns.at(column).kind, m_previewLimit);
}

Result<RowBatch> MySqlCursor::fetch(qsizetype maxRows)
{
    RowBatch batch;
    batch.columnCount = m_columns.size();
    if (m_rowsDone || m_columns.isEmpty())
        return batch;
    for (qsizetype rows = 0; rows < maxRows; ++rows) {
        if (!m_stmt) {
            MYSQL_ROW row = mysql_fetch_row(m_res);
            if (!row) {
                m_rowsDone = true;
                if (mysql_errno(m_mysql) != 0)
                    return fail(connectionError(m_mysql));
                break;
            }
            const unsigned long *lengths = mysql_fetch_lengths(m_res);
            for (qsizetype c = 0; c < m_columns.size(); ++c)
                batch.cells.append(row[c] ? cellValue(c, row[c], lengths[c]) : Value());
            continue;
        }
        const int rc = mysql_stmt_fetch(m_stmt);
        if (rc == MYSQL_NO_DATA) {
            m_rowsDone = true;
            break;
        }
        if (rc == 1) {
            m_rowsDone = true;
            return fail(statementError(m_stmt));
        }
        for (qsizetype c = 0; c < m_columns.size(); ++c) {
            const Cell &cell = m_cells[static_cast<size_t>(c)];
            const ValueKind kind = m_columns.at(c).kind;
            if (cell.isNull) {
                batch.cells.append(Value());
            } else if (kind == ValueKind::Int) {
                batch.cells.append(Value::fromInt(cell.integer));
            } else if (kind == ValueKind::UInt) {
                batch.cells.append(Value::fromUInt(static_cast<quint64>(cell.integer)));
            } else if (kind == ValueKind::Double) {
                batch.cells.append(Value::fromDouble(cell.real));
            } else {
                const auto received = qMin<unsigned long>(
                    cell.length, static_cast<unsigned long>(cell.data.size()));
                Value v = cellValue(c, cell.data.constData(), received);
                if (cell.length > received)
                    v.setTruncated(static_cast<qint64>(cell.length));
                batch.cells.append(std::move(v));
            }
        }
    }
    return batch;
}

void MySqlCursor::releaseResult()
{
    if (m_stmt) {
        if (!m_columns.isEmpty())
            mysql_stmt_free_result(m_stmt); // reads the unread rows
    } else if (m_res) {
        mysql_free_result(m_res); // reads the unread rows (unbuffered result)
    }
    if (m_stmt && m_res)
        mysql_free_result(m_res); // the metadata
    m_res = nullptr;
    m_rowsDone = true;
}

bool MySqlCursor::advance()
{
    for (;;) {
        const bool more = m_stmt ? mysql_stmt_more_results(m_stmt) : mysql_more_results(m_mysql);
        if (!more)
            return false;
        const int rc = m_stmt ? mysql_stmt_next_result(m_stmt) : mysql_next_result(m_mysql);
        if (rc != 0)
            return false;
        if (!openResult())
            return false;
        if (!m_columns.isEmpty())
            return true;
        // A result without rows (a CALL's final status): skip it, keep its affected count.
    }
}

bool MySqlCursor::nextResult()
{
    if (!m_open)
        return false;
    releaseResult();
    if (advance())
        return true;
    finish();
    return false;
}

void MySqlCursor::finish()
{
    if (!m_open)
        return;
    m_open = false;
    m_res = nullptr;
    m_rowsDone = true;
    if (m_stmt) {
        mysql_stmt_close(m_stmt);
        m_stmt = nullptr;
    }
    m_conn.cursorFinished();
}

// Same policy as PostgreSQL (decision 149): a read outside a transaction is killed on the server,
// anything else is read to the end.
void MySqlCursor::close()
{
    if (!m_open)
        return;
    if (!m_rowsDone && m_cancelOnClose)
        sendKill(*m_conn.m_cancel);
    releaseResult();
    while (advance())
        releaseResult();
    finish();
}

} // namespace

Capabilities MySqlDriver::capabilities() const
{
    return {Capability::MultipleDatabases, Capability::DatabaseSwitchInSession,
            Capability::MultipleResultSets, Capability::ServerSideCancel};
}

std::unique_ptr<IConnection> MySqlDriver::createConnection() const
{
    return std::make_unique<MySqlConnection>();
}

} // namespace owl::mysql
