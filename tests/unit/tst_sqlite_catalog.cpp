// SPDX-License-Identifier: GPL-3.0-or-later
// The SQLite catalog on an in-memory fixture schema: tree levels, TableDef (keys, uniques, foreign
// keys, indexes incl. partial / expression ones, triggers, generated columns), DDL.
#include "sqlitedriver.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;

namespace {

ObjectRef ref(QLatin1StringView k, const QString &name)
{
    return {.kind = k, .name = name, .oid = {}};
}

QStringList names(const Result<QList<ObjectNode>> &nodes)
{
    QStringList out;
    if (nodes) {
        for (const ObjectNode &n : *nodes)
            out << n.ref.name;
    }
    return out;
}

} // namespace

class TestSqliteCatalog : public QObject
{
    Q_OBJECT

    sqlite::SqliteDriver m_driver;
    std::unique_ptr<IConnection> m_conn;

    void exec(const QString &sql)
    {
        Statement s;
        s.sql = sql;
        Result<std::unique_ptr<IResultCursor>> c = m_conn->execute(s, {});
        QVERIFY2(c, qPrintable(c ? QString() : c.error().toString()));
        (*c)->close();
    }

    ObjectPath mainPath() const { return ObjectPath{}.child(ref(kind::Database, u"main"_s)); }
    ObjectPath tablePath(const QString &name) const
    {
        return mainPath().child(ref(kind::Group, group::Tables)).child(ref(kind::Table, name));
    }

private slots:
    void initTestCase()
    {
        m_conn = m_driver.createConnection();
        ConnectionParams p;
        p.database = u":memory:"_s;
        QVERIFY(m_conn->open(p));
        exec(u"PRAGMA foreign_keys = ON"_s);
        exec(u"CREATE TABLE customer (id INTEGER PRIMARY KEY, email TEXT NOT NULL UNIQUE, "
             u"name TEXT DEFAULT 'anon', CHECK (length(email) > 3))"_s);
        exec(u"CREATE TABLE orders (customer_id INTEGER NOT NULL REFERENCES customer(id) "
             u"ON DELETE CASCADE, line INTEGER NOT NULL, amount REAL, "
             u"total REAL GENERATED ALWAYS AS (amount * 1.2) VIRTUAL, "
             u"PRIMARY KEY (customer_id, line)) WITHOUT ROWID"_s);
        exec(u"CREATE INDEX orders_big ON orders (amount) WHERE amount > 100"_s);
        exec(u"CREATE INDEX customer_lower ON customer (lower(name))"_s);
        exec(u"CREATE TRIGGER customer_audit AFTER UPDATE ON customer BEGIN SELECT 1; END"_s);
        exec(
            u"CREATE VIEW big_orders AS SELECT customer_id, amount FROM orders WHERE amount > 100"_s);
        exec(u"ATTACH ':memory:' AS other"_s);
        exec(u"CREATE TABLE other.t (x)"_s);
    }

    void cleanupTestCase() { m_conn->close(); }

    void tree()
    {
        ICatalog &c = m_conn->catalog();
        const Result<QList<ObjectNode>> dbs = c.children({});
        QCOMPARE(names(dbs), (QStringList{u"main"_s, u"other"_s})); // temp is empty: hidden
        QCOMPARE(dbs->first().icon, u"database"_s);
        QCOMPARE(names(c.children(mainPath())),
                 (QStringList{u"tables"_s, u"views"_s, u"indexes"_s, u"triggers"_s}));
        QCOMPARE(names(c.children(mainPath().child(ref(kind::Group, group::Tables)))),
                 (QStringList{u"customer"_s, u"orders"_s}));
        QCOMPARE(names(c.children(mainPath().child(ref(kind::Group, group::Views)))),
                 QStringList{u"big_orders"_s});
        const Result<QList<ObjectNode>> indexes
            = c.children(mainPath().child(ref(kind::Group, group::Indexes)));
        QVERIFY(names(indexes).contains(u"orders_big"_s));
        QVERIFY(names(indexes).contains(u"sqlite_autoindex_customer_1"_s)); // UNIQUE (email)

        const ObjectPath customer = tablePath(u"customer"_s);
        QCOMPARE(names(c.children(customer)),
                 (QStringList{u"columns"_s, u"indexes"_s, u"constraints"_s, u"triggers"_s}));
        const Result<QList<ObjectNode>> columns
            = c.children(customer.child(ref(kind::Group, group::Columns)));
        QCOMPARE(names(columns), (QStringList{u"id"_s, u"email"_s, u"name"_s}));
        QCOMPARE(columns->first().icon, u"key"_s);
        QCOMPARE(columns->at(1).badges.value(u"type"_s).toString(), u"TEXT"_s);
        QCOMPARE(names(c.children(customer.child(ref(kind::Group, group::Triggers)))),
                 QStringList{u"customer_audit"_s});
        const Result<QList<ObjectNode>> constraints
            = c.children(tablePath(u"orders"_s).child(ref(kind::Group, group::Constraints)));
        QCOMPARE(constraints->size(), 2); // PRIMARY KEY + the foreign key
        QCOMPARE(constraints->at(1).icon, u"link"_s);
        QCOMPARE(constraints->at(1).displayName, u"(customer_id) → customer(id)"_s);

        QVERIFY(!c.children(mainPath().child(ref(kind::Group, u"nonsense"_s))));
    }

    void tableDefinition()
    {
        const Result<TableDef> customer = m_conn->catalog().table(tablePath(u"customer"_s));
        QVERIFY2(customer, qPrintable(customer ? QString() : customer.error().toString()));
        QCOMPARE(customer->kind, QString(kind::Table));
        QCOMPARE(customer->primaryKey, QStringList{u"id"_s});
        QVERIFY(customer->column(u"id")->autoIncrement); // INTEGER PRIMARY KEY = rowid
        QCOMPARE(customer->column(u"id")->kind, ValueKind::Int);
        QVERIFY(!customer->column(u"email")->nullable);
        QCOMPARE(customer->column(u"name")->defaultExpr, u"'anon'"_s);
        QCOMPARE(customer->uniques.size(), 1);
        QCOMPARE(customer->uniques.first().columns, QStringList{u"email"_s});
        const auto lower
            = std::find_if(customer->indexes.cbegin(), customer->indexes.cend(),
                           [](const IndexDef &i) { return i.name == u"customer_lower"; });
        QVERIFY(lower != customer->indexes.cend());
        QCOMPARE(lower->columns, QStringList{u"<expression>"_s});
        QVERIFY(lower->definition.startsWith(u"CREATE INDEX customer_lower"_s));
        QCOMPARE(customer->triggers.size(), 1);
        QCOMPARE(customer->triggers.first().timing, u"AFTER"_s);
        QCOMPARE(customer->triggers.first().events, QStringList{u"UPDATE"_s});

        const Result<TableDef> orders = m_conn->catalog().table(tablePath(u"orders"_s));
        QVERIFY(orders);
        QCOMPARE(orders->primaryKey, (QStringList{u"customer_id"_s, u"line"_s}));
        QVERIFY(orders->options.value(u"withoutRowid"_s).toBool());
        QVERIFY(!orders->column(u"line")->autoIncrement);
        QCOMPARE(orders->column(u"total")->generatedExpr, u"VIRTUAL"_s);
        QCOMPARE(orders->foreignKeys.size(), 1);
        const ForeignKeyDef &fk = orders->foreignKeys.first();
        QCOMPARE(fk.refTable, u"customer"_s);
        QCOMPARE(fk.columns, QStringList{u"customer_id"_s});
        QCOMPARE(fk.refColumns, QStringList{u"id"_s});
        QCOMPARE(fk.onDelete, u"CASCADE"_s);
        const auto big = std::find_if(orders->indexes.cbegin(), orders->indexes.cend(),
                                      [](const IndexDef &i) { return i.name == u"orders_big"; });
        QCOMPARE(big->predicate, u"amount > 100"_s);

        const ObjectPath view = mainPath()
                                    .child(ref(kind::Group, group::Views))
                                    .child(ref(kind::View, u"big_orders"_s));
        const Result<TableDef> v = m_conn->catalog().table(view);
        QCOMPARE(v->kind, QString(kind::View));
        QCOMPARE(v->columns.size(), 2);

        QVERIFY(!m_conn->catalog().table(tablePath(u"missing"_s)));
    }

    void ddl()
    {
        const Result<QString> customer = m_conn->catalog().ddl(tablePath(u"customer"_s));
        QVERIFY(customer);
        QVERIFY(customer->startsWith(u"CREATE TABLE customer"_s));
        QVERIFY(customer->contains(u"CHECK (length(email) > 3)"_s)); // only visible here
        QVERIFY(customer->contains(u"CREATE INDEX customer_lower"_s));
        QVERIFY(customer->contains(u"CREATE TRIGGER customer_audit"_s));
        const Result<QString> view
            = m_conn->catalog().ddl(mainPath()
                                        .child(ref(kind::Group, group::Views))
                                        .child(ref(kind::View, u"big_orders"_s)));
        QVERIFY(view->startsWith(u"CREATE VIEW big_orders"_s));
        QVERIFY(!m_conn->catalog().ddl(mainPath()));
    }
};

QTEST_GUILESS_MAIN(TestSqliteCatalog)
#include "tst_sqlite_catalog.moc"
