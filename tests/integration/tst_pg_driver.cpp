// SPDX-License-Identifier: GPL-3.0-or-later
// PostgreSQL driver behaviour beyond the shared conformance suite: notices, the type matrix,
// parameter round trips, error details, transaction states, user-defined types, closing partly
// read results, lost connections. Needs a server (tests/support/testserver.h).
#include "pgdriver.h"
#include "testserver.h"

#include <QElapsedTimer>
#include <QTest>

#include <cmath>

using namespace Qt::StringLiterals;
using namespace owl;

namespace {

constexpr qint64 us = 1'000'000;

struct Rows
{
    QList<ColumnInfo> columns;
    QList<QList<Value>> rows;
    qint64 affected = -1;
};

Result<Rows> run(IConnection &conn, const QString &sql, QList<Value> params = {},
                 ExecOptions options = {})
{
    Statement statement;
    statement.sql = sql;
    statement.params = std::move(params);
    Result<std::unique_ptr<IResultCursor>> cursor = conn.execute(statement, options);
    if (!cursor)
        return fail(cursor.error());
    Rows out;
    out.columns = (*cursor)->columns();
    while (!out.columns.isEmpty()) {
        Result<RowBatch> batch = (*cursor)->fetch(1000);
        if (!batch)
            return fail(batch.error());
        if (batch->cells.isEmpty())
            break;
        for (qsizetype r = 0; r < batch->rowCount(); ++r) {
            QList<Value> row;
            for (qsizetype c = 0; c < batch->columnCount; ++c)
                row.append(batch->at(r, c));
            out.rows.append(row);
        }
    }
    out.affected = (*cursor)->affectedRows();
    return out;
}

// The single value of a one-row, one-column query.
Value scalar(IConnection &conn, const QString &sql, QList<Value> params = {})
{
    const Result<Rows> r = run(conn, sql, std::move(params));
    if (!r) {
        qWarning("%s", qPrintable(r.error().toString()));
        return {};
    }
    return r->rows.value(0).value(0);
}

} // namespace

class TestPgDriver : public QObject
{
    Q_OBJECT

    test::TestServer m_server = test::testServer("OWL_TEST_PG_URL", u"postgres"_s);
    postgres::PgDriver m_driver;
    std::unique_ptr<IConnection> m_conn;

    std::unique_ptr<IConnection> connect(const ConnectionParams &params)
    {
        std::unique_ptr<IConnection> conn = m_driver.createConnection();
        if (Result<void> opened = conn->open(params); !opened)
            qWarning("open failed: %s", qPrintable(opened.error().toString()));
        return conn;
    }

private slots:
    void initTestCase()
    {
        if (!m_server.error.isEmpty())
            QFAIL(qPrintable(m_server.error));
        if (!m_server.skipReason.isEmpty())
            QSKIP(qPrintable(m_server.skipReason));
        m_conn = connect(m_server.params);
        QVERIFY(m_conn->isOpen());
        QVERIFY(m_conn->serverInfo().versionNumber >= 130000); // minimum supported (decision 048)
        QVERIFY(run(*m_conn, u"SET TimeZone = 'UTC'"_s));
    }

    // A failed test may leave a transaction open; never let it leak into the next one.
    void init()
    {
        if (m_conn && m_conn->txState() != TxState::Idle)
            (void)run(*m_conn, u"ROLLBACK"_s);
    }

    void cleanupTestCase()
    {
        if (m_conn)
            m_conn->close();
    }

    void notices()
    {
        (void)m_conn->takeMessages();
        QVERIFY(run(*m_conn,
                    u"DO $$BEGIN RAISE NOTICE 'hello %', 42; "
                    u"RAISE WARNING 'careful' USING HINT = 'look'; END$$"_s));
        const QList<ServerMessage> messages = m_conn->takeMessages();
        QCOMPARE(messages.size(), 2);
        QCOMPARE(messages.at(0).severity, ServerMessage::Severity::Notice);
        QCOMPARE(messages.at(0).text, u"hello 42"_s);
        QCOMPARE(messages.at(0).sqlState, u"00000"_s);
        QCOMPARE(messages.at(1).severity, ServerMessage::Severity::Warning);
        QCOMPARE(messages.at(1).text, u"careful\nHINT: look"_s);
        QVERIFY(m_conn->takeMessages().isEmpty());
    }

    void typeMatrix()
    {
        const Result<Rows> r = run(
            *m_conn,
            u"SELECT true, 'NaN'::float8, 12345678901234567890.123456789::numeric, "
            u"1.50::numeric(10,2), 'a'::varchar(10), '2024-02-29'::date, '0044-03-15 BC'::date, "
            u"'13:45:12.123456'::time, '13:45:12+05:30'::timetz, "
            u"'2024-02-29 13:45:12.5'::timestamp, '2024-02-29 13:45:12.5+02'::timestamptz, "
            u"'1 year 2 mons -3 days +04:05:06.5'::interval, "
            u"'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'::uuid, '{\"a\": [1, 2]}'::jsonb, "
            u"ARRAY[1, NULL, 3], ARRAY['x y', 'q\"uote', NULL]::text[], ARRAY[[1,2],[3,4]], "
            u"'192.168.0.1'::inet, 'infinity'::timestamptz, 7::int2, 8::oid"_s);
        QVERIFY2(r, qPrintable(r ? QString() : r.error().toString()));
        const QList<ColumnInfo> &c = r->columns;
        const QList<Value> &v = r->rows.at(0);

        QCOMPARE(v.at(0), Value::fromBool(true));
        QVERIFY(std::isnan(v.at(1).toDouble()));
        QCOMPARE(v.at(2), Value::decimal(u"12345678901234567890.123456789"_s));
        QCOMPARE(v.at(3), Value::decimal(u"1.50"_s));
        QCOMPARE(c.at(3).nativeType, u"numeric(10,2)"_s);
        QCOMPARE(c.at(3).precision, 10);
        QCOMPARE(c.at(3).scale, 2);
        QCOMPARE(c.at(4).nativeType, u"varchar(10)"_s);
        QCOMPARE(c.at(4).length, 10);
        QCOMPARE(v.at(5).toTemporal().date, QDate(2024, 2, 29));
        QCOMPARE(v.at(6).toTemporal().date, QDate(-44, 3, 15));
        QCOMPARE(v.at(7).toTemporal().timeMicros, (13 * 3600 + 45 * 60 + 12) * us + 123456);
        QCOMPARE(v.at(8).kind(), ValueKind::TimeTz);
        QCOMPARE(v.at(8).toTemporal().offsetSeconds, 5 * 3600 + 30 * 60);
        QCOMPARE(v.at(9).kind(), ValueKind::Timestamp);
        QCOMPARE(v.at(9).toTemporal().timeMicros, (13 * 3600 + 45 * 60 + 12) * us + us / 2);
        const Temporal tz = v.at(10).toTemporal(); // shown in the session time zone (UTC)
        QCOMPARE(v.at(10).kind(), ValueKind::TimestampTz);
        QCOMPARE(tz.offsetSeconds, 0);
        QCOMPARE(tz.timeMicros, (11 * 3600 + 45 * 60 + 12) * us + us / 2);
        const Interval iv = v.at(11).toInterval();
        QCOMPARE(iv.months, 14);
        QCOMPARE(iv.days, -3);
        QCOMPARE(iv.micros, (4 * 3600 + 5 * 60 + 6) * us + us / 2);
        QCOMPARE(v.at(12).toUuid(), QUuid(u"a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11"_s));
        QCOMPARE(v.at(13), Value::json(u"{\"a\": [1, 2]}"_s));
        QCOMPARE(c.at(14).nativeType, u"int4[]"_s);
        QCOMPARE(v.at(14), Value::array({Value::fromInt(1), Value(), Value::fromInt(3)}));
        QCOMPARE(v.at(15),
                 Value::array({Value::text(u"x y"_s), Value::text(u"q\"uote"_s), Value()}));
        QCOMPARE(v.at(16).toArray().at(1), Value::array({Value::fromInt(3), Value::fromInt(4)}));
        QCOMPARE(v.at(17), Value::unknown(u"192.168.0.1"_s));
        QCOMPARE(c.at(17).nativeType, u"inet"_s);
        QCOMPARE(v.at(18).toTemporal().special, Temporal::Special::PlusInfinity);
        QCOMPARE(v.at(19), Value::fromInt(7));
        QCOMPARE(v.at(20), Value::fromInt(8));
        QCOMPARE(c.at(0).nativeTypeId, 16u);
    }

    void parameterRoundTrip()
    {
        const Value date = Value::date(QDate(-44, 3, 15));
        const Value stamp = Value::timestampTz(QDate(2024, 2, 29), us * 13 * 3600 + 5, -16200);
        const Value interval
            = Value::interval({.months = 14, .days = -3, .micros = -us, .original = {}});
        const Value array = Value::array({Value::text(u"a\"b,c"_s), Value(), Value::text(u"Ω"_s)});
        const Value bytes = Value::bytes(QByteArray("\x00\xff\x10", 3));
        const Result<Rows> r
            = run(*m_conn,
                  u"SELECT $1::date, $2::timestamptz, $3::interval, $4::text[], "
                  u"$5, $6::float8, $7::numeric, $8::bool, $9::uuid"_s,
                  {date, stamp, interval, array, bytes,
                   Value::fromDouble(std::numeric_limits<double>::infinity()),
                   Value::decimal(u"0.1000000000000000000001"_s), Value::fromBool(false),
                   Value::uuid(QUuid(u"a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11"_s))});
        QVERIFY2(r, qPrintable(r ? QString() : r.error().toString()));
        const QList<Value> &v = r->rows.at(0);
        QCOMPARE(v.at(0).toTemporal().date, QDate(-44, 3, 15));
        const Temporal t = v.at(1).toTemporal(); // converted to UTC by the session
        QCOMPARE(t.date, QDate(2024, 2, 29));
        QCOMPARE(t.timeMicros, (17 * 3600 + 30 * 60) * us + 5);
        QCOMPARE(v.at(2).toInterval().months, 14);
        QCOMPARE(v.at(2).toInterval().days, -3);
        QCOMPARE(v.at(2).toInterval().micros, -us);
        QCOMPARE(v.at(3), array);
        QCOMPARE(r->columns.at(4).nativeType, u"bytea"_s); // bytes are sent typed
        QCOMPARE(v.at(4), bytes);
        QVERIFY(std::isinf(v.at(5).toDouble()));
        QCOMPARE(v.at(6), Value::decimal(u"0.1000000000000000000001"_s));
        QCOMPARE(v.at(7), Value::fromBool(false));
        QCOMPARE(v.at(8).kind(), ValueKind::Uuid);
    }

    void errorDetails()
    {
        QVERIFY(run(*m_conn, u"CREATE TEMP TABLE owl_err (x int PRIMARY KEY)"_s));
        QVERIFY(run(*m_conn, u"INSERT INTO owl_err VALUES (1)"_s));
        const Result<Rows> dup = run(*m_conn, u"INSERT INTO owl_err VALUES (1)"_s);
        QVERIFY(!dup);
        QCOMPARE(dup.error().category, DbError::Category::Constraint);
        QCOMPARE(dup.error().sqlState, u"23505"_s);
        QCOMPARE(dup.error().detail, u"Key (x)=(1) already exists."_s);

        // Positions are UTF-16 offsets even after characters outside the BMP.
        const QString sql = u"SELECT '🦉🦉', no_such_column"_s;
        const Result<Rows> missing = run(*m_conn, sql);
        QVERIFY(!missing);
        QCOMPARE(missing.error().sqlState, u"42703"_s);
        QCOMPARE(missing.error().position, sql.indexOf(u"no_such_column"_s));

        const Result<Rows> perm = run(*m_conn, u"SELECT 1/0"_s);
        QCOMPARE(perm.error().sqlState, u"22012"_s);
        QCOMPARE(perm.error().category, DbError::Category::Other);

        const Result<Rows> two = run(*m_conn, u"SELECT 1; SELECT 2"_s);
        QVERIFY(!two); // one statement per execute() (decision 143)
    }

    void failedTransaction()
    {
        QVERIFY(run(*m_conn, u"BEGIN"_s));
        QVERIFY(!run(*m_conn, u"SELECT 1/0"_s));
        QCOMPARE(m_conn->txState(), TxState::Failed);
        ExecOptions readOnly;
        readOnly.readOnly = true; // must not get in the way of ROLLBACK
        QVERIFY(!run(*m_conn, u"SELECT 1"_s, {}, readOnly)); // 25P02: aborted transaction
        QVERIFY(run(*m_conn, u"ROLLBACK"_s, {}, readOnly));
        QCOMPARE(m_conn->txState(), TxState::Idle);
        // The read-only default stays until a statement runs without the option.
        QVERIFY(!run(*m_conn, u"CREATE TEMP TABLE owl_ro (x int)"_s, {}, readOnly));
        QVERIFY(run(*m_conn, u"CREATE TEMP TABLE owl_ro (x int)"_s));
    }

    void affectedRowsAndReturning()
    {
        QVERIFY(run(*m_conn, u"CREATE TEMP TABLE owl_ret (id serial, x int)"_s));
        const Result<Rows> ins
            = run(*m_conn, u"INSERT INTO owl_ret (x) VALUES (10), (20) RETURNING id, x"_s);
        QVERIFY(ins);
        QCOMPARE(ins->rows.size(), 2);
        QCOMPARE(ins->affected, 2);
        const Result<Rows> sel = run(*m_conn, u"SELECT * FROM owl_ret"_s);
        QCOMPARE(sel->affected, -1); // reads report no affected rows
        const Result<Rows> ddl = run(*m_conn, u"CREATE TEMP TABLE owl_ret2 (x int)"_s);
        QCOMPARE(ddl->affected, -1);
        QVERIFY(ddl->columns.isEmpty());
    }

    // Types created after the connection loaded pg_type are looked up before the next statement.
    void userDefinedTypes()
    {
        QVERIFY(run(*m_conn, u"BEGIN"_s));
        QVERIFY(run(*m_conn, u"CREATE TYPE owl_mood AS ENUM ('sad', 'happy')"_s));
        QVERIFY(run(*m_conn, u"CREATE DOMAIN owl_posint AS int CHECK (VALUE > 0)"_s));
        const QString sql = u"SELECT 'happy'::owl_mood, 5::owl_posint, ARRAY['sad']::owl_mood[], "
                            u"c FROM pg_class c LIMIT 1"_s;
        const Result<Rows> first = run(*m_conn, sql);
        QVERIFY(first);
        QCOMPARE(first->rows.at(0).at(0).toText(), u"happy"_s); // unknown yet: server text
        const Result<Rows> again = run(*m_conn, sql);
        QVERIFY(again);
        QCOMPARE(again->columns.at(0).nativeType, u"owl_mood"_s);
        QCOMPARE(again->columns.at(0).kind, ValueKind::Unknown); // enums are edited as text
        // The server describes domain columns with their base type.
        QCOMPARE(again->columns.at(1).nativeType, u"int4"_s);
        QCOMPARE(again->rows.at(0).at(1), Value::fromInt(5));
        QCOMPARE(again->columns.at(2).nativeType, u"owl_mood[]"_s);
        QCOMPARE(again->rows.at(0).at(2), Value::array({Value::unknown(u"sad"_s)}));
        QCOMPARE(again->columns.at(3).nativeType, u"pg_class"_s); // composite row type
        QVERIFY(run(*m_conn, u"ROLLBACK"_s));
    }

    // Closing a paused read outside a transaction cancels it on the server. (The server acts on a
    // cancel at its next interrupt check: per page in table scans, per row with a filter or
    // projection — a bare FROM generate_series() is read from a tuplestore without any.)
    void closePausedReadCancels()
    {
        Statement statement;
        statement.sql = u"SELECT x FROM (SELECT generate_series(1, 50000000) AS x) s WHERE x > 0"_s;
        Result<std::unique_ptr<IResultCursor>> cursor = m_conn->execute(statement, {});
        QVERIFY(cursor);
        const Result<RowBatch> batch = (*cursor)->fetch(10);
        QVERIFY(batch);
        QCOMPARE(batch->rowCount(), 10);
        QElapsedTimer t;
        t.start();
        (*cursor)->close();
        qInfo("closed after %lld ms", static_cast<long long>(t.elapsed()));
        QVERIFY(t.elapsed() < 3000); // reading 50 M rows would take far longer
        QCOMPARE(scalar(*m_conn, u"SELECT 7"_s), Value::fromInt(7));
        QCOMPARE(m_conn->txState(), TxState::Idle);
    }

    // Inside a transaction the rest is read instead, so the transaction stays usable.
    void closePausedReadInTransactionDrains()
    {
        QVERIFY(run(*m_conn, u"BEGIN"_s));
        Statement statement;
        statement.sql = u"SELECT x FROM generate_series(1, 100000) AS x"_s;
        Result<std::unique_ptr<IResultCursor>> cursor = m_conn->execute(statement, {});
        QVERIFY(cursor);
        QVERIFY((*cursor)->fetch(10));
        QCOMPARE(m_conn->txState(), TxState::InTransaction);
        (*cursor)->close();
        QCOMPARE(m_conn->txState(), TxState::InTransaction);
        QCOMPARE(scalar(*m_conn, u"SELECT 8"_s), Value::fromInt(8));
        QVERIFY(run(*m_conn, u"COMMIT"_s));
    }

    void busyConnectionRefusesSecondStatement()
    {
        Statement statement;
        statement.sql = u"SELECT x FROM generate_series(1, 10000) AS x"_s;
        Result<std::unique_ptr<IResultCursor>> cursor = m_conn->execute(statement, {});
        QVERIFY(cursor);
        QVERIFY(m_conn->ping());
        const Result<Rows> second = run(*m_conn, u"SELECT 1"_s);
        QVERIFY(!second);
        QCOMPARE(second.error().category, DbError::Category::Unsupported);
        (*cursor)->close();
        QVERIFY(run(*m_conn, u"SELECT 1"_s));
    }

    void copyIsUnsupported()
    {
        const Result<Rows> out = run(*m_conn, u"COPY (SELECT 1) TO STDOUT"_s);
        QVERIFY(!out);
        QCOMPARE(out.error().category, DbError::Category::Unsupported);
        QVERIFY(run(*m_conn, u"CREATE TEMP TABLE owl_copy (x int)"_s));
        const Result<Rows> in = run(*m_conn, u"COPY owl_copy FROM STDIN"_s);
        QVERIFY(!in);
        QCOMPARE(scalar(*m_conn, u"SELECT count(*) FROM owl_copy"_s), Value::fromInt(0));
    }

    void lostConnection()
    {
        std::unique_ptr<IConnection> victim = connect(m_server.params);
        QVERIFY(victim->isOpen());
        const Value pid = scalar(*victim, u"SELECT pg_backend_pid()"_s);
        QCOMPARE(scalar(*m_conn, u"SELECT pg_terminate_backend($1::int)"_s, {pid}),
                 Value::fromBool(true));
        QTRY_VERIFY(!victim->ping());
        const Result<Rows> r = run(*victim, u"SELECT 1"_s);
        QVERIFY(!r);
        QCOMPARE(r.error().category, DbError::Category::Lost);
    }

    void connectionOptions()
    {
        ConnectionParams p = m_server.params;
        p.driverOptions.insert(u"options"_s, u"-c search_path=pg_catalog"_s);
        std::unique_ptr<IConnection> conn = connect(p);
        QVERIFY(conn->isOpen());
        QCOMPARE(scalar(*conn, u"SHOW search_path"_s), Value::text(u"pg_catalog"_s));

        p.driverOptions = {{u"no_such_option"_s, 1}};
        Result<void> bad = m_driver.createConnection()->open(p);
        QVERIFY(!bad);
        QCOMPARE(bad.error().category, DbError::Category::Connection);
    }

    void wrongPassword()
    {
        ConnectionParams p = m_server.params;
        p.password = u"definitely-wrong"_s;
        Result<void> r = m_driver.createConnection()->open(p);
        QVERIFY(!r);
        qInfo("%s", qPrintable(r.error().toString()));
        QCOMPARE(r.error().category, DbError::Category::Auth);
    }

    void unreachableServer()
    {
        ConnectionParams p = m_server.params;
        p.host = u"127.0.0.1"_s;
        p.port = 1; // nothing listens there
        Result<void> r = m_driver.createConnection()->open(p);
        QVERIFY(!r);
        QCOMPARE(r.error().category, DbError::Category::Connection);
    }
};

QTEST_GUILESS_MAIN(TestPgDriver)
#include "tst_pg_driver.moc"
