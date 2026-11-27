// SPDX-License-Identifier: GPL-3.0-or-later
// MySQL / MariaDB driver behaviour beyond the shared conformance suite: the type matrix through
// both protocols, column origins, parameter round trips, multiple result sets, warnings, error
// details, transactions, closing partly read results, lost connections, TLS modes. Runs against
// MySQL 8.4 / 5.7 and MariaDB 11.8 / 10.6 (tests/support/testserver.h).
#include "mysqldriver.h"
#include "testserver.h"

#include <QElapsedTimer>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;

namespace {

constexpr qint64 us = 1'000'000;

struct Rows
{
    QList<ColumnInfo> columns;
    QList<QList<Value>> rows;
    qint64 affected = -1;
    std::optional<Value> insertId;
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
    out.insertId = (*cursor)->lastInsertId();
    (*cursor)->close();
    return out;
}

Value scalar(IConnection &conn, const QString &sql, QList<Value> params = {})
{
    const Result<Rows> r = run(conn, sql, std::move(params));
    if (!r) {
        qWarning("%s", qPrintable(r.error().toString()));
        return {};
    }
    return r->rows.value(0).value(0);
}

// n rows from cross-joined digit tables (no CTEs: MySQL 5.7), streamed without sorting.
QString manyRows(int digitTables)
{
    const QString digit = u"(SELECT 0 AS d UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT "
                          u"3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL "
                          u"SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9)"_s;
    QStringList from;
    for (int i = 0; i < digitTables; ++i)
        from << u"%1 t%2"_s.arg(digit).arg(i);
    return u"SELECT t0.d FROM "_s + from.join(u", "_s);
}

} // namespace

class TestMySqlDriver : public QObject
{
    Q_OBJECT

    test::TestServer m_server = test::testServer("OWL_TEST_MYSQL_URL", u"mysql"_s);
    mysql::MySqlDriver m_driver;
    std::unique_ptr<IConnection> m_conn;
    bool m_mariadb = false;

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
        const ServerInfo info = m_conn->serverInfo();
        qInfo("%s %s (%d)", qPrintable(info.product), qPrintable(info.version), info.versionNumber);
        m_mariadb = info.product == u"MariaDB"_s;
        QVERIFY(info.versionNumber >= (m_mariadb ? 100600 : 50700)); // decision 048
        QVERIFY(!info.version.contains(u"MariaDB"_s));
        QVERIFY(run(*m_conn, u"SET time_zone = '+00:00'"_s));
    }

    void init()
    {
        if (m_conn && m_conn->txState() != TxState::Idle)
            (void)run(*m_conn, u"ROLLBACK"_s);
        (void)m_conn->takeMessages();
    }

    void cleanupTestCase()
    {
        if (m_conn)
            m_conn->close();
    }

    // The same row through the text protocol and the binary protocol (prepared statement).
    void typeMatrixBothProtocols()
    {
        QVERIFY(run(*m_conn,
                    u"CREATE TEMPORARY TABLE owl_types ("
                    u"id INT NOT NULL PRIMARY KEY, ti TINYINT, bu BIGINT UNSIGNED, "
                    u"f FLOAT, d DOUBLE, amount DECIMAL(10,2), vc VARCHAR(20), ch CHAR(3), "
                    u"tx TEXT, bl BLOB, vb VARBINARY(10), dt DATE, dtm DATETIME(6), "
                    u"ts TIMESTAMP(3) NULL, tm TIME, tmneg TIME, yr YEAR, js JSON, "
                    u"bt BIT(8), en ENUM('a','b'), st SET('x','y'), nul INT) "
                    u"DEFAULT CHARSET = utf8mb4"_s)); // 5.7 defaults to latin1
        QVERIFY(run(*m_conn,
                    u"INSERT INTO owl_types VALUES (1, -5, 18446744073709551615, 1.5, 2.25, "
                    u"12345678.91, 'Ωmega 🦉', 'ab', REPEAT('t', 10), X'00FF', X'0102', "
                    u"'2024-02-29', '2024-02-29 13:45:12.123456', '2024-02-29 13:45:12.5', "
                    u"'13:45:12', '-838:59:59', 2024, '{\"a\": [1, 2]}', b'10100101', 'b', 'x,y', "
                    u"NULL)"_s));

        const Result<Rows> text = run(*m_conn, u"SELECT * FROM owl_types"_s);
        QVERIFY2(text, qPrintable(text ? QString() : text.error().toString()));
        const Result<Rows> binary
            = run(*m_conn, u"SELECT * FROM owl_types WHERE id = ?"_s, {Value::fromInt(1)});
        QVERIFY2(binary, qPrintable(binary ? QString() : binary.error().toString()));

        for (const Result<Rows> *r : {&text, &binary}) {
            const QList<ColumnInfo> &c = (*r)->columns;
            const QList<Value> &v = (*r)->rows.at(0);
            QCOMPARE(v.at(1), Value::fromInt(-5));
            QCOMPARE(c.at(1).nativeType, u"tinyint"_s);
            QCOMPARE(v.at(2), Value::fromUInt(std::numeric_limits<quint64>::max()));
            QCOMPARE(c.at(2).nativeType, u"bigint unsigned"_s);
            QCOMPARE(v.at(3), Value::fromDouble(1.5));
            QCOMPARE(v.at(4), Value::fromDouble(2.25));
            QCOMPARE(v.at(5), Value::decimal(u"12345678.91"_s));
            QCOMPARE(c.at(5).nativeType, u"decimal(10,2)"_s);
            QCOMPARE(c.at(5).precision, 10);
            QCOMPARE(v.at(6), Value::text(u"Ωmega 🦉"_s));
            QCOMPARE(c.at(6).nativeType, u"varchar(20)"_s);
            QCOMPARE(v.at(7), Value::text(u"ab"_s));
            QCOMPARE(c.at(7).nativeType, u"char(3)"_s);
            QCOMPARE(v.at(8), Value::text(u"tttttttttt"_s));
            QCOMPARE(c.at(8).nativeType, u"text"_s);
            QCOMPARE(v.at(9), Value::bytes(QByteArray("\x00\xff", 2)));
            QCOMPARE(c.at(9).nativeType, u"blob"_s);
            QCOMPARE(v.at(10), Value::bytes(QByteArray("\x01\x02", 2)));
            QCOMPARE(c.at(10).nativeType, u"varbinary(10)"_s);
            QCOMPARE(v.at(11).toTemporal().date, QDate(2024, 2, 29));
            const Temporal dtm = v.at(12).toTemporal();
            QCOMPARE(v.at(12).kind(), ValueKind::Timestamp);
            QCOMPARE(dtm.timeMicros, (13 * 3600 + 45 * 60 + 12) * us + 123456);
            QCOMPARE(c.at(12).nativeType, u"datetime(6)"_s);
            QCOMPARE(v.at(13).toTemporal().timeMicros, (13 * 3600 + 45 * 60 + 12) * us + us / 2);
            QCOMPARE(c.at(13).nativeType, u"timestamp(3)"_s);
            QCOMPARE(v.at(14).toTemporal().timeMicros, (13 * 3600 + 45 * 60 + 12) * us);
            QCOMPARE(v.at(15).kind(), ValueKind::Interval); // TIME beyond a day is a duration
            QCOMPARE(v.at(15).toInterval().micros, -(838 * 3600 + 59 * 60 + 59) * us);
            QCOMPARE(v.at(16), Value::fromInt(2024));
            QCOMPARE(c.at(17).kind, ValueKind::Json);
            QVERIFY(v.at(17).toText().contains(u"\"a\""_s));
            QCOMPARE(v.at(18), Value::bytes(QByteArray("\xa5", 1)));
            QCOMPARE(c.at(18).nativeType, u"bit(8)"_s);
            QCOMPARE(v.at(19), Value::text(u"b"_s));
            QCOMPARE(c.at(19).nativeType, u"enum"_s);
            QCOMPARE(v.at(20), Value::text(u"x,y"_s));
            QCOMPARE(c.at(20).nativeType, u"set"_s);
            QVERIFY(v.at(21).isNull());

            // Column origins make query results editable (specs/data-grid.md §5.1).
            QCOMPARE(c.at(0).table, u"owl_types"_s);
            QCOMPARE(c.at(0).tableColumn, u"id"_s);
            QCOMPARE(c.at(0).schema, m_server.params.database);
            QVERIFY(c.at(0).isPrimaryKey);
            QVERIFY(!c.at(0).nullable);
            QVERIFY(c.at(1).nullable);
        }
        QCOMPARE(text->rows, binary->rows);
    }

    void parameterRoundTrip()
    {
        const Result<Rows> r = run(
            *m_conn,
            u"SELECT ?, ?, ?, ?, CAST(? AS DATE), CAST(? AS DATETIME(6)), CAST(? AS "
            u"DECIMAL(30,10)), "
            u"?, ?"_s,
            {Value::text(u"it's \\ \"q\""_s), Value::fromUInt(std::numeric_limits<quint64>::max()),
             Value::fromDouble(0.1), Value::fromBool(true), Value::date(QDate(2024, 2, 29)),
             Value::timestamp(QDate(2024, 2, 29), us * 13 * 3600 + 5),
             Value::decimal(u"12345678901234567890.0123456789"_s),
             Value::bytes(QByteArray("\x00\x01\xff", 3)), Value()});
        QVERIFY2(r, qPrintable(r ? QString() : r.error().toString()));
        const QList<Value> &v = r->rows.at(0);
        QCOMPARE(v.at(0).toText(), u"it's \\ \"q\""_s);
        QCOMPARE(v.at(1).toUInt(), std::numeric_limits<quint64>::max());
        QCOMPARE(v.at(2).toDouble(), 0.1);
        QCOMPARE(v.at(3).toInt(), 1);
        QCOMPARE(v.at(4).toTemporal().date, QDate(2024, 2, 29));
        QCOMPARE(v.at(5).toTemporal().timeMicros, us * 13 * 3600 + 5);
        QCOMPARE(v.at(6), Value::decimal(u"12345678901234567890.0123456789"_s));
        QCOMPARE(v.at(7).toBytes(), QByteArray("\x00\x01\xff", 3));
        QVERIFY(v.at(8).isNull());
    }

    void multipleResultSets()
    {
        QVERIFY(run(*m_conn, u"DROP PROCEDURE IF EXISTS owl_two"_s));
        QVERIFY(run(*m_conn,
                    u"CREATE PROCEDURE owl_two(IN n INT) BEGIN "
                    u"SELECT n AS a; SELECT n + 1 AS b, n + 2 AS c; END"_s));
        for (const bool prepared : {false, true}) {
            Statement statement;
            statement.sql = prepared ? u"CALL owl_two(?)"_s : u"CALL owl_two(1)"_s;
            if (prepared)
                statement.params = {Value::fromInt(1)};
            Result<std::unique_ptr<IResultCursor>> cursor = m_conn->execute(statement, {});
            QVERIFY2(cursor, qPrintable(cursor ? QString() : cursor.error().toString()));
            IResultCursor &c = **cursor;
            QCOMPARE(c.columns().size(), 1);
            QCOMPARE(c.fetch(10)->at(0, 0).toInt(), 1);
            QVERIFY(c.nextResult());
            QCOMPARE(c.columns().size(), 2);
            const Result<RowBatch> second = c.fetch(10);
            QCOMPARE(second->at(0, 1).toInt(), 3);
            QVERIFY(!c.nextResult()); // the CALL's own status is not a result set
            c.close();
            QCOMPARE(scalar(*m_conn, u"SELECT 5"_s).toInt(), 5); // connection is free again
        }
        QVERIFY(run(*m_conn, u"DROP PROCEDURE owl_two"_s));
    }

    void warnings()
    {
        QCOMPARE(scalar(*m_conn, u"SELECT CAST('12abc' AS SIGNED)"_s).toInt(), 12);
        const QList<ServerMessage> messages = m_conn->takeMessages();
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.at(0).severity, ServerMessage::Severity::Warning);
        QVERIFY2(messages.at(0).text.contains(u"(1292)"_s), qPrintable(messages.at(0).text));
    }

    void errorDetails()
    {
        const QString sql = u"SELECT 1,\n  2 FROM WHERE"_s;
        const Result<Rows> syntax = run(*m_conn, sql);
        QVERIFY(!syntax);
        QCOMPARE(syntax.error().category, DbError::Category::Syntax);
        QCOMPARE(syntax.error().nativeCode, 1064);
        QCOMPARE(syntax.error().position, sql.indexOf(u"WHERE"_s)); // on line 2

        const Result<Rows> missing = run(*m_conn, u"SELECT * FROM owl_no_such_table"_s);
        QCOMPARE(missing.error().sqlState, u"42S02"_s);
        QCOMPARE(missing.error().category, DbError::Category::Syntax);

        QVERIFY(run(*m_conn, u"CREATE TEMPORARY TABLE owl_err (x INT PRIMARY KEY)"_s));
        QVERIFY(run(*m_conn, u"INSERT INTO owl_err VALUES (1)"_s));
        const Result<Rows> dup = run(*m_conn, u"INSERT INTO owl_err VALUES (1)"_s);
        QCOMPARE(dup.error().category, DbError::Category::Constraint);
        QCOMPARE(dup.error().nativeCode, 1062);
        QCOMPARE(dup.error().sqlState, u"23000"_s);

        QVERIFY(!run(*m_conn, u"SELECT 1; SELECT 2"_s)); // one statement per execute()
        QVERIFY(m_conn->takeMessages().isEmpty()); // errors are not repeated as warnings
    }

    void transactions()
    {
        QVERIFY(run(*m_conn, u"BEGIN"_s));
        QCOMPARE(m_conn->txState(), TxState::InTransaction);
        QVERIFY(!run(*m_conn, u"SELECT * FROM owl_no_such_table"_s));
        QCOMPARE(m_conn->txState(), TxState::InTransaction); // MySQL has no failed state
        ExecOptions readOnly;
        readOnly.readOnly = true;
        QVERIFY(run(*m_conn, u"ROLLBACK"_s, {}, readOnly));
        QCOMPARE(m_conn->txState(), TxState::Idle);
        QVERIFY(!run(*m_conn, u"CREATE TEMPORARY TABLE owl_ro (x INT)"_s, {}, readOnly));
        QVERIFY(run(*m_conn, u"CREATE TEMPORARY TABLE owl_ro (x INT)"_s));
    }

    void affectedRowsAndInsertId()
    {
        QVERIFY(run(*m_conn,
                    u"CREATE TEMPORARY TABLE owl_ai (id INT AUTO_INCREMENT PRIMARY KEY, "
                    u"x INT)"_s));
        const Result<Rows> ins = run(*m_conn, u"INSERT INTO owl_ai (x) VALUES (10), (20)"_s);
        QVERIFY(ins);
        QCOMPARE(ins->affected, 2);
        QCOMPARE(ins->insertId, Value::fromUInt(1)); // the first id of a multi-row insert
        const Result<Rows> prep
            = run(*m_conn, u"INSERT INTO owl_ai (x) VALUES (?)"_s, {Value::fromInt(30)});
        QCOMPARE(prep->affected, 1);
        QCOMPARE(prep->insertId, Value::fromUInt(3));
        QCOMPARE(run(*m_conn, u"SELECT * FROM owl_ai"_s)->affected, -1);
    }

    void databaseSwitchAndExecutableComments()
    {
        QVERIFY(run(*m_conn, u"USE mysql"_s));
        QCOMPARE(scalar(*m_conn, u"SELECT DATABASE()"_s).toText(), u"mysql"_s);
        QVERIFY(run(*m_conn, u"USE `%1`"_s.arg(m_server.params.database)));
        QVERIFY(run(*m_conn, u"/*!40101 SET @owl_x = 41 + 1 */"_s)); // runs on the server
        QCOMPARE(scalar(*m_conn, u"SELECT @owl_x"_s).toInt(), 42);
        const Result<Rows> empty = run(*m_conn, u"-- nothing\n/* at all */"_s);
        QVERIFY(empty);
        QVERIFY(empty->columns.isEmpty());
    }

    void closePausedReadKills()
    {
        Statement statement;
        statement.sql = manyRows(8); // 100 M rows
        Result<std::unique_ptr<IResultCursor>> cursor = m_conn->execute(statement, {});
        QVERIFY(cursor);
        QCOMPARE((*cursor)->fetch(10)->rowCount(), 10);
        QElapsedTimer t;
        t.start();
        (*cursor)->close();
        qInfo("closed after %lld ms", static_cast<long long>(t.elapsed()));
        QVERIFY(t.elapsed() < 3000);
        QCOMPARE(scalar(*m_conn, u"SELECT 7"_s).toInt(), 7);
    }

    void closePausedReadInTransactionDrains()
    {
        QVERIFY(run(*m_conn, u"BEGIN"_s));
        Statement statement;
        statement.sql = manyRows(5); // 100 000 rows
        Result<std::unique_ptr<IResultCursor>> cursor = m_conn->execute(statement, {});
        QVERIFY(cursor);
        QVERIFY((*cursor)->fetch(10));
        (*cursor)->close();
        QCOMPARE(m_conn->txState(), TxState::InTransaction);
        QCOMPARE(scalar(*m_conn, u"SELECT 8"_s).toInt(), 8);
        QVERIFY(run(*m_conn, u"COMMIT"_s));
    }

    void busyConnectionRefusesSecondStatement()
    {
        Statement statement;
        statement.sql = manyRows(4);
        Result<std::unique_ptr<IResultCursor>> cursor = m_conn->execute(statement, {});
        QVERIFY(cursor);
        QVERIFY(m_conn->ping());
        const Result<Rows> second = run(*m_conn, u"SELECT 1"_s);
        QCOMPARE(second.error().category, DbError::Category::Unsupported);
        (*cursor)->close();
        QVERIFY(run(*m_conn, u"SELECT 1"_s));
    }

    void lostConnection()
    {
        std::unique_ptr<IConnection> victim = connect(m_server.params);
        QVERIFY(victim->isOpen());
        const Value id = scalar(*victim, u"SELECT CONNECTION_ID()"_s); // unsigned on MySQL
        const quint64 threadId
            = id.kind() == ValueKind::UInt ? id.toUInt() : static_cast<quint64>(id.toInt());
        QVERIFY(threadId != 0);
        QVERIFY(run(*m_conn, u"KILL %1"_s.arg(threadId)));
        QTRY_VERIFY(!victim->ping());
        const Result<Rows> r = run(*victim, u"SELECT 1"_s);
        QVERIFY(!r);
        qInfo("%s", qPrintable(r.error().toString()));
        QCOMPARE(r.error().category, DbError::Category::Lost);
    }

    void tlsModes()
    {
        ConnectionParams p = m_server.params;
        p.ssl.mode = SslOptions::Mode::Disable;
        std::unique_ptr<IConnection> plain = connect(p);
        QVERIFY(plain->isOpen());
        const Result<Rows> status = run(*plain, u"SHOW SESSION STATUS LIKE 'Ssl_cipher'"_s);
        QCOMPARE(status->rows.at(0).at(1).toText(), QString());

        p.ssl.mode = SslOptions::Mode::Require;
        Result<void> secure = m_driver.createConnection()->open(p);
        qInfo("TLS required: %s", secure ? "connected" : qPrintable(secure.error().toString()));
        if (!secure) // servers without TLS (MariaDB < 11.4 in Docker) must refuse, not downgrade
            QCOMPARE(secure.error().category, DbError::Category::Connection);
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
        p.port = 1;
        Result<void> r = m_driver.createConnection()->open(p);
        QVERIFY(!r);
        QCOMPARE(r.error().category, DbError::Category::Connection);
    }
};

QTEST_GUILESS_MAIN(TestMySqlDriver)
#include "tst_mysql_driver.moc"
