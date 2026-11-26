// SPDX-License-Identifier: GPL-3.0-or-later
// SqlDialect behaviour through the SQLite dialect: splitting, classification, placeholders,
// quoting (specs/driver-api.md §5, specs/sql-editor.md §3).
#include "sqlitedialect.h"

#include <QTest>

using namespace Qt::StringLiterals;
using owl::StatementClass;

Q_DECLARE_METATYPE(owl::StatementClass)

class TestSqliteDialect : public QObject
{
    Q_OBJECT

    owl::sqlite::SqliteDialect m_dialect;

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

        QTest::newRow("simple") << u"SELECT 1; SELECT 2"_s
                                << QStringList{u"SELECT 1"_s, u"SELECT 2"_s};
        QTest::newRow("trailing semicolon and blanks")
            << u"  SELECT 1 ;\n\n ; \n"_s << QStringList{u"SELECT 1"_s};
        QTest::newRow("semicolons in strings")
            << u"SELECT 'a;b', 'it''s; fine'; SELECT 2"_s
            << QStringList{u"SELECT 'a;b', 'it''s; fine'"_s, u"SELECT 2"_s};
        QTest::newRow("semicolons in identifiers")
            << u"SELECT \"x;y\", `a;b`, [c;d] FROM t; SELECT 2"_s
            << QStringList{u"SELECT \"x;y\", `a;b`, [c;d] FROM t"_s, u"SELECT 2"_s};
        QTest::newRow("semicolons in comments")
            << u"SELECT 1 -- a; b\n; /* c; d */ SELECT 2"_s
            << QStringList{u"SELECT 1 -- a; b"_s, u"SELECT 2"_s};
        QTest::newRow("comment-only statement dropped")
            << u"-- just a note;\n/* and; this */"_s << QStringList{};
        QTest::newRow("trigger body keeps its semicolons")
            << u"CREATE TRIGGER t AFTER INSERT ON a BEGIN\n"
               u"  UPDATE b SET n = n + 1;\n"
               u"  INSERT INTO log VALUES (CASE WHEN 1 THEN 'x' END);\n"
               u"END; SELECT 1"_s
            << QStringList{u"CREATE TRIGGER t AFTER INSERT ON a BEGIN\n"
                           u"  UPDATE b SET n = n + 1;\n"
                           u"  INSERT INTO log VALUES (CASE WHEN 1 THEN 'x' END);\n"
                           u"END"_s,
                           u"SELECT 1"_s};
        QTest::newRow("temp trigger")
            << u"CREATE TEMP TRIGGER t BEFORE DELETE ON a BEGIN SELECT 1; END;"_s
            << QStringList{u"CREATE TEMP TRIGGER t BEFORE DELETE ON a BEGIN SELECT 1; END"_s};
        QTest::newRow("BEGIN TRANSACTION is a plain statement")
            << u"BEGIN; INSERT INTO a VALUES (1); COMMIT;"_s
            << QStringList{u"BEGIN"_s, u"INSERT INTO a VALUES (1)"_s, u"COMMIT"_s};
        QTest::newRow("unterminated string swallows the rest")
            << u"SELECT 'oops; SELECT 2"_s << QStringList{u"SELECT 'oops; SELECT 2"_s};
        QTest::newRow("unicode") << u"SELECT 'Ωmega 🦉'; SELECT 2"_s
                                 << QStringList{u"SELECT 'Ωmega 🦉'"_s, u"SELECT 2"_s};
    }

    void split()
    {
        QFETCH(QString, script);
        QFETCH(QStringList, statements);
        QCOMPARE(split(script), statements);
    }

    void spansAreOffsetsIntoTheScript()
    {
        const QString script = u"  SELECT 1;\nUPDATE t SET x = 1"_s;
        const QList<owl::StatementSpan> spans = m_dialect.split(script);
        QCOMPARE(spans.size(), 2);
        QCOMPARE(spans.at(0).start, 2);
        QCOMPARE(spans.at(0).length, 8);
        QCOMPARE(spans.at(1).start, 12);
    }

    void classify_data()
    {
        QTest::addColumn<QString>("sql");
        QTest::addColumn<StatementClass>("expected");
        QTest::newRow("select") << u"select * from t"_s << StatementClass::Read;
        QTest::newRow("parenthesised select")
            << u"(SELECT 1) UNION (SELECT 2)"_s << StatementClass::Read;
        QTest::newRow("leading comment")
            << u"-- report\n/* x */ SELECT 1"_s << StatementClass::Read;
        QTest::newRow("values") << u"VALUES (1), (2)"_s << StatementClass::Read;
        QTest::newRow("with select")
            << u"WITH a AS (SELECT 1) SELECT * FROM a"_s << StatementClass::Read;
        QTest::newRow("with delete bounded")
            << u"WITH a AS (SELECT 1) DELETE FROM t WHERE id IN (SELECT * FROM a)"_s
            << StatementClass::Write;
        QTest::newRow("with delete unbounded")
            << u"WITH a AS (SELECT 1) DELETE FROM t"_s << StatementClass::WriteUnbounded;
        QTest::newRow("data-modifying CTE")
            << u"WITH d AS (DELETE FROM t WHERE x RETURNING *) SELECT * FROM d"_s
            << StatementClass::Write;
        QTest::newRow("insert") << u"INSERT INTO t VALUES (1)"_s << StatementClass::Write;
        QTest::newRow("replace") << u"REPLACE INTO t VALUES (1)"_s << StatementClass::Write;
        QTest::newRow("update with where")
            << u"UPDATE t SET a = 1 WHERE id = 2"_s << StatementClass::Write;
        QTest::newRow("update without where")
            << u"UPDATE t SET a = 1"_s << StatementClass::WriteUnbounded;
        QTest::newRow("WHERE only in subquery")
            << u"UPDATE t SET a = (SELECT b FROM u WHERE u.id = 1)"_s
            << StatementClass::WriteUnbounded;
        QTest::newRow("WHERE inside string")
            << u"DELETE FROM t -- WHERE id = 1"_s << StatementClass::WriteUnbounded;
        QTest::newRow("delete with where")
            << u"delete from t where id = 1"_s << StatementClass::Write;
        QTest::newRow("delete without where")
            << u"DELETE FROM t"_s << StatementClass::WriteUnbounded;
        QTest::newRow("explain") << u"EXPLAIN QUERY PLAN SELECT 1"_s << StatementClass::Read;
        QTest::newRow("explain delete is not executed")
            << u"EXPLAIN DELETE FROM t"_s << StatementClass::Read;
        QTest::newRow("create") << u"CREATE TABLE t (x)"_s << StatementClass::Ddl;
        QTest::newRow("drop") << u"DROP TABLE t"_s << StatementClass::Ddl;
        QTest::newRow("alter") << u"ALTER TABLE t ADD COLUMN y"_s << StatementClass::Ddl;
        QTest::newRow("begin") << u"BEGIN IMMEDIATE"_s << StatementClass::Tx;
        QTest::newRow("commit") << u"COMMIT"_s << StatementClass::Tx;
        QTest::newRow("end") << u"END TRANSACTION"_s << StatementClass::Tx;
        QTest::newRow("savepoint") << u"SAVEPOINT a"_s << StatementClass::Tx;
        QTest::newRow("pragma read") << u"PRAGMA table_info(t)"_s << StatementClass::Read;
        QTest::newRow("pragma write") << u"PRAGMA foreign_keys = ON"_s << StatementClass::Other;
        QTest::newRow("vacuum") << u"VACUUM"_s << StatementClass::Write;
        QTest::newRow("attach") << u"ATTACH 'x.db' AS x"_s << StatementClass::Other;
        QTest::newRow("empty") << u"  -- nothing"_s << StatementClass::Other;
    }

    void classify()
    {
        QFETCH(QString, sql);
        QFETCH(StatementClass, expected);
        QCOMPARE(m_dialect.classify(sql), expected);
    }

    void placeholders()
    {
        const QString sql = u"SELECT ?, ?2, :name, @other, $third, '?:not', \"col?\" -- ?x\n"_s;
        const QList<owl::Placeholder> found = m_dialect.placeholders(sql);
        QStringList names;
        for (const owl::Placeholder &p : found)
            names << p.name;
        QCOMPARE(names, (QStringList{u""_s, u"2"_s, u"name"_s, u"other"_s, u"third"_s}));
        QCOMPARE(sql.mid(found.at(2).start, found.at(2).length), u":name"_s);
    }

    void quoting()
    {
        QCOMPARE(m_dialect.quoteIdentifier(u"order"_s), u"\"order\""_s);
        QCOMPARE(m_dialect.quoteIdentifier(u"a\"b"_s), u"\"a\"\"b\""_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value()), u"NULL"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::text(u"it's"_s)), u"'it''s'"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::fromInt(-42)), u"-42"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::fromBool(true)), u"TRUE"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::decimal(u"1.50"_s)), u"1.50"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::bytes(QByteArray("\x00\xff", 2))),
                 u"X'00ff'"_s);
    }

    void tokenizerMarksUnterminated()
    {
        const QList<owl::SqlToken> tokens = m_dialect.tokenize(u"SELECT 'abc"_s);
        QVERIFY(tokens.last().unterminated);
        QCOMPARE(tokens.last().type, owl::SqlToken::Type::String);
        const QList<owl::SqlToken> comment = m_dialect.tokenize(u"/* open"_s);
        QVERIFY(comment.last().unterminated);
    }
};

QTEST_GUILESS_MAIN(TestSqliteDialect)
#include "tst_sqlite_dialect.moc"
