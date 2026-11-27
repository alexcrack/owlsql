// SPDX-License-Identifier: GPL-3.0-or-later
// The MySQL / MariaDB dialect: DELIMITER scripts, stored-program bodies, MySQL lexing ("x" strings,
// backslash escapes, # and "-- " comments), ? placeholders, quoting, INTO OUTFILE.
#include "mysqldialect.h"

#include <QTest>

using namespace Qt::StringLiterals;
using owl::StatementClass;

Q_DECLARE_METATYPE(owl::StatementClass)

class TestMySqlDialect : public QObject
{
    Q_OBJECT

    owl::mysql::MySqlDialect m_dialect;

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

        QTest::newRow("simple") << u"SELECT 1; SELECT 2;"_s
                                << QStringList{u"SELECT 1"_s, u"SELECT 2"_s};
        QTest::newRow("double-quoted strings with backslash escapes")
            << u"SELECT \"a;\\\"b\", 'c\\';d'; SELECT 2"_s
            << QStringList{u"SELECT \"a;\\\"b\", 'c\\';d'"_s, u"SELECT 2"_s};
        QTest::newRow("hash and dash comments")
            << u"SELECT 1 # a; b\n; SELECT 2 -- c; d\n; SELECT 3"_s
            << QStringList{u"SELECT 1 # a; b"_s, u"SELECT 2 -- c; d"_s, u"SELECT 3"_s};
        QTest::newRow("--1 is not a comment")
            << u"SELECT 5 --1; SELECT 2"_s << QStringList{u"SELECT 5 --1"_s, u"SELECT 2"_s};
        QTest::newRow("DELIMITER")
            << u"DELIMITER //\n"
               u"CREATE PROCEDURE p() BEGIN SELECT 1; SELECT 2; END //\n"
               u"DELIMITER ;\n"
               u"CALL p();\n"_s
            << QStringList{u"CREATE PROCEDURE p() BEGIN SELECT 1; SELECT 2; END"_s, u"CALL p()"_s};
        QTest::newRow("DELIMITER $$ and a string containing it")
            << u"DELIMITER $$\nSELECT '$$'$$\nSELECT 2$$"_s
            << QStringList{u"SELECT '$$'"_s, u"SELECT 2"_s};
        QTest::newRow("procedure body without DELIMITER")
            << u"CREATE DEFINER = `root`@`%` PROCEDURE p(IN n INT)\n"
               u"BEGIN\n"
               u"  DECLARE i INT DEFAULT 0;\n"
               u"  WHILE i < n DO SET i = i + 1; END WHILE;\n"
               u"  IF i > 0 THEN SELECT CASE WHEN i > 1 THEN 'many' ELSE 'one' END; END IF;\n"
               u"  CASE i WHEN 1 THEN SELECT 1; ELSE BEGIN END; END CASE;\n"
               u"  lbl: LOOP LEAVE lbl; END LOOP lbl;\n"
               u"END;\n"
               u"SELECT 2"_s
            << QStringList{u"CREATE DEFINER = `root`@`%` PROCEDURE p(IN n INT)\n"
                           u"BEGIN\n"
                           u"  DECLARE i INT DEFAULT 0;\n"
                           u"  WHILE i < n DO SET i = i + 1; END WHILE;\n"
                           u"  IF i > 0 THEN SELECT CASE WHEN i > 1 THEN 'many' ELSE 'one' END; "
                           u"END IF;\n"
                           u"  CASE i WHEN 1 THEN SELECT 1; ELSE BEGIN END; END CASE;\n"
                           u"  lbl: LOOP LEAVE lbl; END LOOP lbl;\n"
                           u"END"_s,
                           u"SELECT 2"_s};
        QTest::newRow("trigger with a single statement body")
            << u"CREATE TRIGGER t BEFORE INSERT ON a FOR EACH ROW SET NEW.x = 1; SELECT 2"_s
            << QStringList{u"CREATE TRIGGER t BEFORE INSERT ON a FOR EACH ROW SET NEW.x = 1"_s,
                           u"SELECT 2"_s};
        QTest::newRow("MariaDB anonymous block")
            << u"BEGIN NOT ATOMIC SELECT 1; SELECT 2; END; SELECT 3"_s
            << QStringList{u"BEGIN NOT ATOMIC SELECT 1; SELECT 2; END"_s, u"SELECT 3"_s};
        QTest::newRow("BEGIN as a transaction")
            << u"BEGIN; UPDATE t SET x = 1 WHERE id = 2; COMMIT"_s
            << QStringList{u"BEGIN"_s, u"UPDATE t SET x = 1 WHERE id = 2"_s, u"COMMIT"_s};
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
        QTest::newRow("select into outfile")
            << u"SELECT * FROM t INTO OUTFILE '/tmp/x'"_s << StatementClass::Write;
        QTest::newRow("select into variable") << u"SELECT 1 INTO @x"_s << StatementClass::Read;
        QTest::newRow("replace") << u"REPLACE INTO t VALUES (1)"_s << StatementClass::Write;
        QTest::newRow("show") << u"SHOW TABLES"_s << StatementClass::Read;
        QTest::newRow("describe") << u"DESCRIBE t"_s << StatementClass::Read;
        QTest::newRow("use") << u"USE db"_s << StatementClass::Other;
        QTest::newRow("call") << u"CALL p()"_s << StatementClass::Write;
        QTest::newRow("explain format")
            << u"EXPLAIN FORMAT=JSON DELETE FROM t"_s << StatementClass::Read;
        QTest::newRow("explain analyze")
            << u"EXPLAIN ANALYZE DELETE FROM t"_s << StatementClass::WriteUnbounded;
        QTest::newRow("delete with where in a string")
            << u"DELETE FROM t # WHERE id = 1"_s << StatementClass::WriteUnbounded;
    }

    void classify()
    {
        QFETCH(QString, sql);
        QFETCH(StatementClass, expected);
        QCOMPARE(m_dialect.classify(sql), expected);
    }

    void placeholders()
    {
        const QString sql = u"SELECT ?, '?', \"?\", `?`, @var, :name FROM t WHERE a = ? # ?\n"_s;
        QCOMPARE(m_dialect.placeholders(sql).size(), 2);
    }

    void quoting()
    {
        QCOMPARE(m_dialect.quoteIdentifier(u"or`der"_s), u"`or``der`"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::text(u"it's a \\ path"_s)),
                 u"'it''s a \\\\ path'"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::bytes(QByteArray("\x00\xff", 2))),
                 u"X'00ff'"_s);
        QCOMPARE(m_dialect.quoteLiteral(owl::Value::fromBool(false)), u"FALSE"_s);
    }
};

QTEST_GUILESS_MAIN(TestMySqlDialect)
#include "tst_mysql_dialect.moc"
