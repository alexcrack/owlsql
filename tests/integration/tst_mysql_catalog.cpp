// SPDX-License-Identifier: GPL-3.0-or-later
// The MySQL / MariaDB catalog on a fixture database (owl_cat): every tree level, TableDef details
// (defaults as SQL expressions on both flavours, prefix indexes, generated columns, checks where
// supported) and DDL — the table DDL is executed again in another database to prove it is valid.
#include "mysqldriver.h"
#include "testserver.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;

namespace {

ObjectRef ref(QLatin1StringView k, const QString &name)
{
    return {.kind = k, .name = name, .oid = {}};
}

QStringList names(const QList<ObjectNode> &nodes)
{
    QStringList out;
    for (const ObjectNode &n : nodes)
        out << n.ref.name;
    return out;
}

const ObjectNode *byName(const QList<ObjectNode> &&nodes, const QString &name) = delete;
const ObjectNode *byName(const QList<ObjectNode> &nodes, const QString &name)
{
    for (const ObjectNode &n : nodes) {
        if (n.ref.name == name)
            return &n;
    }
    return nullptr;
}

const QStringList fixture{
    u"DROP DATABASE IF EXISTS owl_cat"_s,
    u"DROP DATABASE IF EXISTS owl_cat2"_s,
    u"CREATE DATABASE owl_cat CHARACTER SET utf8mb4"_s,
    u"CREATE TABLE owl_cat.customer (id INT AUTO_INCREMENT PRIMARY KEY, "
    u"email VARCHAR(100) NOT NULL UNIQUE COMMENT 'Login', name VARCHAR(50) DEFAULT 'anon', "
    u"score DECIMAL(5,2) DEFAULT 1.50, created DATETIME DEFAULT CURRENT_TIMESTAMP, "
    u"KEY customer_name (name(10)), CONSTRAINT email_check CHECK (CHAR_LENGTH(email) > 3)) "
    u"ENGINE = InnoDB COMMENT = 'People'"_s,
    u"CREATE TABLE owl_cat.orders (customer_id INT NOT NULL, line INT NOT NULL, "
    u"amount DECIMAL(10,2), total DECIMAL(12,2) AS (amount * 1.2) STORED, "
    u"PRIMARY KEY (customer_id, line), CONSTRAINT orders_customer FOREIGN KEY (customer_id) "
    u"REFERENCES owl_cat.customer (id) ON DELETE CASCADE) ENGINE = InnoDB"_s,
    u"CREATE VIEW owl_cat.big_orders AS SELECT customer_id, amount FROM owl_cat.orders "
    u"WHERE amount > 100"_s,
    u"CREATE FUNCTION owl_cat.add_one(x INT) RETURNS INT DETERMINISTIC RETURN x + 1"_s,
    u"CREATE PROCEDURE owl_cat.noop() BEGIN SELECT 1; END"_s,
    u"CREATE TRIGGER owl_cat.customer_touch BEFORE UPDATE ON owl_cat.customer FOR EACH ROW "
    u"SET NEW.name = TRIM(NEW.name)"_s,
    u"CREATE EVENT owl_cat.cleanup ON SCHEDULE EVERY 1 DAY "
    u"DO DELETE FROM owl_cat.orders WHERE amount < 0"_s,
};

} // namespace

class TestMySqlCatalog : public QObject
{
    Q_OBJECT

    test::TestServer m_server = test::testServer("OWL_TEST_MYSQL_URL", u"mysql"_s);
    mysql::MySqlDriver m_driver;
    std::unique_ptr<IConnection> m_conn;
    bool m_checks = false; // CHECK constraints reported (MySQL 8.0.16+, MariaDB)

    void exec(const QString &sql)
    {
        Statement s;
        s.sql = sql;
        Result<std::unique_ptr<IResultCursor>> c = m_conn->execute(s, {});
        QVERIFY2(c, qPrintable(sql + u": "_s + (c ? QString() : c.error().toString())));
        (*c)->close();
    }

    ICatalog &cat() { return m_conn->catalog(); }
    ObjectPath dbPath() const { return ObjectPath{}.child(ref(kind::Database, u"owl_cat"_s)); }
    ObjectPath groupPath(QLatin1StringView group) const
    {
        return dbPath().child(ref(kind::Group, group));
    }
    ObjectPath tablePath(const QString &name) const
    {
        return groupPath(group::Tables).child(ref(kind::Table, name));
    }

private slots:
    void initTestCase()
    {
        if (!m_server.error.isEmpty())
            QFAIL(qPrintable(m_server.error));
        if (!m_server.skipReason.isEmpty())
            QSKIP(qPrintable(m_server.skipReason));
        m_conn = m_driver.createConnection();
        QVERIFY(m_conn->open(m_server.params));
        const ServerInfo info = m_conn->serverInfo();
        m_checks = info.product == u"MariaDB"_s || info.versionNumber >= 80016;
        for (const QString &sql : fixture)
            exec(sql);
    }

    void cleanupTestCase()
    {
        if (m_conn && m_conn->isOpen()) {
            exec(u"DROP DATABASE IF EXISTS owl_cat"_s);
            exec(u"DROP DATABASE IF EXISTS owl_cat2"_s);
            m_conn->close();
        }
    }

    void databasesAndGroups()
    {
        const Result<QList<ObjectNode>> dbs = cat().children({});
        QVERIFY2(dbs, qPrintable(dbs ? QString() : dbs.error().toString()));
        QVERIFY(byName(*dbs, u"owl_cat"_s));
        QVERIFY(byName(*dbs, u"mysql"_s)->isSystem);
        QVERIFY(byName(*dbs, u"information_schema"_s)->isSystem);
        QVERIFY(byName(*dbs, m_server.params.database)->badges.value(u"current"_s).toBool());
        QCOMPARE(byName(*dbs, u"owl_cat"_s)->badges.value(u"charset"_s).toString(), u"utf8mb4"_s);
        QCOMPARE(names(*cat().children(dbPath())),
                 (QStringList{u"tables"_s, u"views"_s, u"functions"_s, u"procedures"_s,
                              u"triggers"_s, u"events"_s}));

        const Result<QList<ObjectNode>> tables = cat().children(groupPath(group::Tables));
        QCOMPARE(names(*tables), (QStringList{u"customer"_s, u"orders"_s}));
        QCOMPARE(tables->first().badges.value(u"engine"_s).toString(), u"InnoDB"_s);
        QCOMPARE(tables->first().badges.value(u"comment"_s).toString(), u"People"_s);
        QCOMPARE(names(*cat().children(groupPath(group::Views))), QStringList{u"big_orders"_s});
        QCOMPARE(names(*cat().children(groupPath(group::Functions))), QStringList{u"add_one"_s});
        QCOMPARE(names(*cat().children(groupPath(group::Procedures))), QStringList{u"noop"_s});
        const Result<QList<ObjectNode>> triggers = cat().children(groupPath(group::Triggers));
        QCOMPARE(names(*triggers), QStringList{u"customer_touch"_s});
        QCOMPARE(triggers->first().badges.value(u"table"_s).toString(), u"customer"_s);
        QCOMPARE(names(*cat().children(groupPath(group::Events))), QStringList{u"cleanup"_s});
    }

    void tableGroups()
    {
        const ObjectPath customer = tablePath(u"customer"_s);
        QCOMPARE(names(*cat().children(customer)),
                 (QStringList{u"columns"_s, u"indexes"_s, u"constraints"_s, u"triggers"_s}));
        const Result<QList<ObjectNode>> columns
            = cat().children(customer.child(ref(kind::Group, group::Columns)));
        QCOMPARE(names(*columns),
                 (QStringList{u"id"_s, u"email"_s, u"name"_s, u"score"_s, u"created"_s}));
        QCOMPARE(columns->first().icon, u"key"_s);
        QCOMPARE(names(*cat().children(customer.child(ref(kind::Group, group::Indexes)))),
                 (QStringList{u"PRIMARY"_s, u"customer_name"_s, u"email"_s}));
        const QStringList constraints
            = names(*cat().children(customer.child(ref(kind::Group, group::Constraints))));
        QCOMPARE(constraints.mid(0, 2), (QStringList{u"PRIMARY"_s, u"email"_s}));
        QCOMPARE(constraints.contains(u"email_check"_s), m_checks);
        QCOMPARE(names(*cat().children(customer.child(ref(kind::Group, group::Triggers)))),
                 QStringList{u"customer_touch"_s});
        const ObjectPath view = groupPath(group::Views).child(ref(kind::View, u"big_orders"_s));
        QCOMPARE(names(*cat().children(view)), QStringList{u"columns"_s});
    }

    void tableDefinitions()
    {
        const Result<TableDef> customer = cat().table(tablePath(u"customer"_s));
        QVERIFY2(customer, qPrintable(customer ? QString() : customer.error().toString()));
        QCOMPARE(customer->schema, u"owl_cat"_s);
        QCOMPARE(customer->comment, u"People"_s);
        QCOMPARE(customer->options.value(u"engine"_s).toString(), u"InnoDB"_s);
        QCOMPARE(customer->primaryKeyName, u"PRIMARY"_s);
        QCOMPARE(customer->primaryKey, QStringList{u"id"_s});
        QVERIFY(customer->column(u"id")->autoIncrement);
        QCOMPARE(customer->column(u"id")->kind, ValueKind::Int);
        const ColumnDef *email = customer->column(u"email");
        QCOMPARE(email->type, u"varchar(100)"_s);
        QVERIFY(!email->nullable);
        QCOMPARE(email->comment, u"Login"_s);
        // Defaults are SQL expressions on both flavours.
        QCOMPARE(customer->column(u"name")->defaultExpr, u"'anon'"_s);
        QCOMPARE(customer->column(u"score")->defaultExpr, u"1.50"_s);
        QCOMPARE(customer->column(u"score")->kind, ValueKind::Decimal);
        QVERIFY(customer->column(u"created")
                    ->defaultExpr.contains(u"current_timestamp"_s, Qt::CaseInsensitive));
        QVERIFY(customer->column(u"email")->defaultExpr.isEmpty());
        QCOMPARE(customer->uniques.size(), 1);
        QCOMPARE(customer->uniques.first().columns, QStringList{u"email"_s});
        const auto prefix
            = std::find_if(customer->indexes.cbegin(), customer->indexes.cend(),
                           [](const IndexDef &i) { return i.name == u"customer_name"; });
        QCOMPARE(prefix->columns, QStringList{u"name(10)"_s});
        QCOMPARE(prefix->method, u"BTREE"_s);
        QCOMPARE(customer->checks.size(), m_checks ? 1 : 0);
        if (m_checks)
            QVERIFY(customer->checks.first().expression.contains(u"char_length"_s,
                                                                 Qt::CaseInsensitive));
        QCOMPARE(customer->triggers.size(), 1);
        QCOMPARE(customer->triggers.first().timing, u"BEFORE"_s);
        QCOMPARE(customer->triggers.first().events, QStringList{u"UPDATE"_s});

        const Result<TableDef> orders = cat().table(tablePath(u"orders"_s));
        QVERIFY(orders);
        QCOMPARE(orders->primaryKey, (QStringList{u"customer_id"_s, u"line"_s}));
        QVERIFY(orders->column(u"total")->generatedExpr.contains(u"amount"_s));
        QVERIFY(orders->column(u"total")->defaultExpr.isEmpty());
        QCOMPARE(orders->foreignKeys.size(), 1);
        const ForeignKeyDef &fk = orders->foreignKeys.first();
        QCOMPARE(fk.name, u"orders_customer"_s);
        QCOMPARE(fk.refSchema, u"owl_cat"_s);
        QCOMPARE(fk.refTable, u"customer"_s);
        QCOMPARE(fk.refColumns, QStringList{u"id"_s});
        QCOMPARE(fk.onDelete, u"CASCADE"_s);
        QVERIFY(fk.onUpdate == u"RESTRICT"_s || fk.onUpdate == u"NO ACTION"_s);

        const Result<TableDef> view
            = cat().table(groupPath(group::Views).child(ref(kind::View, u"big_orders"_s)));
        QCOMPARE(view->kind, QString(kind::View));
        QCOMPARE(view->columns.size(), 2);
        QVERIFY(!cat().table(tablePath(u"missing"_s)));
    }

    void ddl()
    {
        const Result<QString> customer = cat().ddl(tablePath(u"customer"_s));
        QVERIFY2(customer, qPrintable(customer ? QString() : customer.error().toString()));
        QVERIFY(customer->startsWith(u"CREATE TABLE `customer`"_s));
        // Triggers: MySQL 8 normalises the text, 5.7 and MariaDB keep it as written.
        QVERIFY2(customer->contains(u"customer_touch"_s)
                     && customer->contains(u"BEFORE UPDATE ON"_s),
                 qPrintable(*customer));

        // Valid DDL: create the table again in another database (the trigger names owl_cat).
        exec(u"CREATE DATABASE owl_cat2"_s);
        exec(u"USE owl_cat2"_s);
        const QList<StatementSpan> statements = m_driver.dialect().split(*customer);
        QCOMPARE(statements.size(), 2);
        exec(statements.first().in(*customer).toString());
        exec(u"USE `%1`"_s.arg(m_server.params.database));

        QVERIFY(cat()
                    .ddl(groupPath(group::Views).child(ref(kind::View, u"big_orders"_s)))
                    ->contains(u"VIEW `owl_cat`.`big_orders` AS"_s));
        QVERIFY(cat()
                    .ddl(groupPath(group::Functions).child(ref(kind::Function, u"add_one"_s)))
                    ->contains(u"FUNCTION `add_one`"_s));
        QVERIFY(cat()
                    .ddl(groupPath(group::Procedures).child(ref(kind::Procedure, u"noop"_s)))
                    ->contains(u"PROCEDURE `noop`"_s));
        QVERIFY(cat()
                    .ddl(groupPath(group::Events).child(ref(kind::Event, u"cleanup"_s)))
                    ->contains(u"EVENT `cleanup`"_s));
        QVERIFY(cat()
                    .ddl(groupPath(group::Triggers).child(ref(kind::Trigger, u"customer_touch"_s)))
                    ->contains(u"customer_touch"_s));
        QVERIFY(cat().ddl(dbPath())->startsWith(u"CREATE DATABASE `owl_cat`"_s));

        const ObjectPath customerPath = tablePath(u"customer"_s);
        QCOMPARE(*cat().ddl(customerPath.child(ref(kind::Group, group::Indexes))
                                .child(ref(kind::Index, u"customer_name"_s))),
                 u"CREATE INDEX `customer_name` ON `owl_cat`.`customer` (`name`(10));\n"_s);
        QCOMPARE(*cat().ddl(tablePath(u"orders"_s)
                                .child(ref(kind::Group, group::Constraints))
                                .child(ref(kind::Constraint, u"orders_customer"_s))),
                 u"ALTER TABLE `owl_cat`.`orders` ADD CONSTRAINT `orders_customer` FOREIGN KEY "
                 u"(`customer_id`) REFERENCES `owl_cat`.`customer` (`id`) ON UPDATE "_s
                     + cat().table(tablePath(u"orders"_s))->foreignKeys.first().onUpdate
                     + u" ON DELETE CASCADE;\n"_s);
        QVERIFY(!cat().ddl(customerPath.child(ref(kind::Group, group::Columns))));
    }
};

QTEST_GUILESS_MAIN(TestMySqlCatalog)
#include "tst_mysql_catalog.moc"
