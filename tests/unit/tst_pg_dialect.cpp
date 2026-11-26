// SPDX-License-Identifier: GPL-3.0-or-later
// The PostgreSQL dialect: dollar quotes, nested comments, E'' strings, BEGIN ATOMIC bodies,
// $n placeholders, SELECT INTO, literal quoting (specs/driver-api.md §5).
#include "pgdialect.h"

#include <QTest>

#include <limits>

using namespace Qt::StringLiterals;
using owl::StatementClass;

Q_DECLARE_METATYPE(owl::StatementClass)

class TestPgDialect : public QObject
{
    Q_OBJECT

    owl::postgres::PgDialect m_dialect;

    QStringList split(const QString &script) const
    {
        QStringList out;
        for (const owl::StatementSpan &span : m_dialect.split(script))
            out << span.in(script).toString();
        return out;
    }

private slots:
    void split_data()
    {
        QTest::addColumn<QString>("script");
        QTest::addColumn<QStringList>("statements");

        QTest::newRow("dollar-quoted function body")
            << u"CREATE FUNCTION f() RETURNS int AS $$ BEGIN RETURN 1; END; $$ LANGUAGE plpgsql;"
               u" SELECT f()"_s
            << QStringList{u"CREATE FUNCTION f() RETURNS int AS $$ BEGIN RETURN 1; END; $$ "
                           u"LANGUAGE plpgsql"_s,
                           u"SELECT f()"_s};
        QTest::newRow("tagged dollar quotes nest")
            << u"DO $outer$ BEGIN EXECUTE $q$SELECT 1; SELECT 2$q$; END $outer$; SELECT 3"_s
            << QStringList{u"DO $outer$ BEGIN EXECUTE $q$SELECT 1; SELECT 2$q$; END $outer$"_s,
                           u"SELECT 3"_s};
        QTest::newRow("nested block comments")
            << u"SELECT 1 /* a /* b; */ c; */; SELECT 2"_s
            << QStringList{u"SELECT 1 /* a /* b; */ c; */"_s, u"SELECT 2"_s};
        QTest::newRow("E string with backslash quote")
            << u"SELECT E'it\\'s; ok'; SELECT 2"_s
            << QStringList{u"SELECT E'it\\'s; ok'"_s, u"SELECT 2"_s};
        QTest::newRow("standard string: backslash is literal")
            << u"SELECT 'C:\\'; SELECT 2"_s << QStringList{u"SELECT 'C:\\'"_s, u"SELECT 2"_s};
        QTest::newRow("BEGIN ATOMIC body")
            << u"CREATE FUNCTION f() RETURNS int LANGUAGE sql BEGIN ATOMIC SELECT 1; "
               u"SELECT CASE WHEN true THEN 2 END; END; SELECT 3"_s
            << QStringList{u"CREATE FUNCTION f() RETURNS int LANGUAGE sql BEGIN ATOMIC SELECT 1; "
                           u"SELECT CASE WHEN true THEN 2 END; END"_s,
                           u"SELECT 3"_s};
        QTest::newRow("trigger is a plain statement")
            << u"CREATE TRIGGER t BEFORE INSERT ON a FOR EACH ROW EXECUTE FUNCTION f(); "
               u"BEGIN; SELECT 1; COMMIT"_s
            << QStringList{
                   u"CREATE TRIGGER t BEFORE INSERT ON a FOR EACH ROW EXECUTE FUNCTION f()"_s,
                   u"BEGIN"_s, u"SELECT 1"_s, u"COMMIT"_s};
        QTest::newRow("$1 is not a dollar quote")
            << u"SELECT $1; SELECT $2"_s << QStringList{u"SELECT $1"_s, u"SELECT $2"_s};
    }

    void split()
    {
        QFETCH(QString, script);
        QFETCH(QStringList, statements);
        QCOMPARE(split(script), statements);
    }

    void classify_data()
    {
        QTest::addColumn<QString>("sql");
        QTest::addColumn<StatementClass>("expected");
        QTest::newRow("select") << u"SELECT * FROM t"_s << StatementClass::Read;
        QTest::newRow("select into") << u"SELECT * INTO new_t FROM t"_s << StatementClass::Ddl;
        QTest::newRow("with select into")
            << u"WITH a AS (SELECT 1) SELECT * INTO b FROM a"_s << StatementClass::Ddl;
        QTest::newRow("INTO in a subquery string")
            << u"SELECT 'INTO', (SELECT 1) FROM t"_s << StatementClass::Read;
        QTest::newRow("insert into") << u"INSERT INTO t VALUES (1)"_s << StatementClass::Write;
        QTest::newRow("copy to") << u"COPY t TO STDOUT"_s << StatementClass::Read;
        QTest::newRow("copy from") << u"COPY t FROM STDIN"_s << StatementClass::Write;
        QTest::newRow("truncate") << u"TRUNCATE t"_s << StatementClass::WriteUnbounded;
        QTest::newRow("do block") << u"DO $$BEGIN DELETE FROM t; END$$"_s << StatementClass::Write;
        QTest::newRow("explain analyze delete")
            << u"EXPLAIN (ANALYZE) DELETE FROM t"_s << StatementClass::WriteUnbounded;
        QTest::newRow("explain analyze with options")
            << u"EXPLAIN (ANALYZE, TIMING OFF, FORMAT JSON) UPDATE t SET a = 1 WHERE b"_s
            << StatementClass::Write;
        QTest::newRow("explain with options only plans")
            << u"EXPLAIN (COSTS OFF, VERBOSE) DELETE FROM t"_s << StatementClass::Read;
        QTest::newRow("start transaction") << u"START TRANSACTION"_s << StatementClass::Tx;
        QTest::newRow("set") << u"SET search_path = a"_s << StatementClass::Other;
        QTest::newRow("show") << u"SHOW search_path"_s << StatementClass::Read;
        QTest::newRow("refresh") << u"REFRESH MATERIALIZED VIEW v"_s << StatementClass::Write;
    }

    void classify()
    {
        QFETCH(QString, sql);
        QFETCH(StatementClass, expected);
        QCOMPARE(m_dialect.classify(sql), expected);
    }

    void placeholders()
    {
        const QString sql
            = u"SELECT $1, $12, data ? 'k', data ?| array['a'], x::int, arr[1:2], '$3', $$ $4 $$"_s;
        QStringList names;
        for (const owl::Placeholder &p : m_dialect.placeholders(sql))
            names << p.name;
        QCOMPARE(names, (QStringList{u"1"_s, u"12"_s}));
    }

    void quoting()
    {
        QCOMPARE(m_dialect.quoteIdentifier(u"User"_s), u"\"User\""_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::bytes(QByteArray("\x00\xff", 2))),
                 u"'\\x00ff'::bytea"_s);
        QCOMPARE(
            m_dialect.quoteLiteral(owl::Value::fromDouble(std::numeric_limits<double>::infinity())),
            u"'Infinity'::float8"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::text(u"it's"_s)), u"'it''s'"_s);
    }
};

QTEST_GUILESS_MAIN(TestPgDialect)
#include "tst_pg_dialect.moc"
