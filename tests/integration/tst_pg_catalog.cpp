// SPDX-License-Identifier: GPL-3.0-or-later
// The PostgreSQL catalog on a fixture schema (owl_cat): every tree level, TableDef details and the
// DDL of each object kind — the generated table DDL is executed again to prove it is valid.
#include "pgdriver.h"
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
    u"DROP SCHEMA IF EXISTS owl_cat CASCADE"_s,
    u"DROP SCHEMA IF EXISTS owl_cat2 CASCADE"_s,
    u"CREATE SCHEMA owl_cat"_s,
    u"COMMENT ON SCHEMA owl_cat IS 'catalog fixture'"_s,
    u"CREATE TYPE owl_cat.mood AS ENUM ('sad', 'happy')"_s,
    u"CREATE DOMAIN owl_cat.posint AS int CHECK (VALUE > 0)"_s,
    u"CREATE TABLE owl_cat.customer (id int GENERATED ALWAYS AS IDENTITY PRIMARY KEY, "
    u"email text NOT NULL UNIQUE, name varchar(50) COLLATE \"C\" DEFAULT 'anon', "
    u"mood owl_cat.mood, CHECK (length(email) > 3))"_s,
    u"COMMENT ON TABLE owl_cat.customer IS 'People'"_s,
    u"COMMENT ON COLUMN owl_cat.customer.email IS 'Login'"_s,
    u"CREATE TABLE owl_cat.orders (customer_id int NOT NULL REFERENCES owl_cat.customer (id) "
    u"ON DELETE CASCADE, line int NOT NULL, amount numeric(10,2), "
    u"total numeric GENERATED ALWAYS AS (amount * 1.2) STORED, PRIMARY KEY (customer_id, line))"_s,
    u"CREATE INDEX orders_big ON owl_cat.orders (amount) WHERE amount > 100"_s,
    u"CREATE INDEX customer_lower ON owl_cat.customer (lower(name))"_s,
    u"CREATE FUNCTION owl_cat.touch() RETURNS trigger LANGUAGE plpgsql "
    u"AS $$BEGIN RETURN NEW; END$$"_s,
    u"CREATE FUNCTION owl_cat.add(a int, b int) RETURNS int LANGUAGE sql AS 'SELECT a + b'"_s,
    u"CREATE FUNCTION owl_cat.add(a numeric, b numeric) RETURNS numeric LANGUAGE sql "
    u"AS 'SELECT a + b'"_s,
    u"CREATE PROCEDURE owl_cat.noop() LANGUAGE sql AS 'SELECT 1'"_s,
    u"CREATE TRIGGER customer_touch BEFORE UPDATE ON owl_cat.customer FOR EACH ROW "
    u"EXECUTE FUNCTION owl_cat.touch()"_s,
    u"CREATE VIEW owl_cat.big_orders AS SELECT customer_id, amount FROM owl_cat.orders "
    u"WHERE amount > 100"_s,
    u"CREATE MATERIALIZED VIEW owl_cat.order_totals AS SELECT customer_id, sum(amount) AS total "
    u"FROM owl_cat.orders GROUP BY customer_id"_s,
    u"CREATE INDEX order_totals_customer ON owl_cat.order_totals (customer_id)"_s,
    u"CREATE SEQUENCE owl_cat.ticket START 100"_s,
    u"CREATE TABLE owl_cat.events (id int, at date) PARTITION BY RANGE (at)"_s,
    u"CREATE TABLE owl_cat.events_2024 PARTITION OF owl_cat.events "
    u"FOR VALUES FROM ('2024-01-01') TO ('2025-01-01')"_s,
};

} // namespace

class TestPgCatalog : public QObject
{
    Q_OBJECT

    test::TestServer m_server = test::testServer("OWL_TEST_PG_URL", u"postgres"_s);
    postgres::PgDriver m_driver;
    std::unique_ptr<IConnection> m_conn;
    QString m_db;

    void exec(const QString &sql)
    {
        Statement s;
        s.sql = sql;
        Result<std::unique_ptr<IResultCursor>> c = m_conn->execute(s, {});
        QVERIFY2(c, qPrintable(sql + u": "_s + (c ? QString() : c.error().toString())));
        (*c)->close();
    }

    ICatalog &cat() { return m_conn->catalog(); }
    ObjectPath dbPath() const { return ObjectPath{}.child(ref(kind::Database, m_db)); }
    ObjectPath schemaPath() const { return dbPath().child(ref(kind::Schema, u"owl_cat"_s)); }
    ObjectPath groupPath(QLatin1StringView group) const
    {
        return schemaPath().child(ref(kind::Group, group));
    }
    // A node as the tree hands it out (with its OID).
    ObjectPath nodePath(QLatin1StringView group, const QString &name)
    {
        const Result<QList<ObjectNode>> nodes = cat().children(groupPath(group));
        if (!nodes)
            return {};
        const ObjectNode *n = byName(*nodes, name);
        return n ? groupPath(group).child(n->ref) : ObjectPath{};
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
        m_db = m_server.params.database;
        for (const QString &sql : fixture)
            exec(sql);
    }

    void cleanupTestCase()
    {
        if (m_conn && m_conn->isOpen()) {
            exec(u"DROP SCHEMA IF EXISTS owl_cat CASCADE"_s);
            exec(u"DROP SCHEMA IF EXISTS owl_cat2 CASCADE"_s);
            m_conn->close();
        }
    }

    void databasesAndSchemas()
    {
        const Result<QList<ObjectNode>> dbs = cat().children({});
        QVERIFY2(dbs, qPrintable(dbs ? QString() : dbs.error().toString()));
        const ObjectNode *own = byName(*dbs, m_db);
        QVERIFY(own);
        QVERIFY(own->badges.value(u"current"_s).toBool());
        QVERIFY(!own->ref.oid.isEmpty());

        const Result<QList<ObjectNode>> other
            = cat().children(ObjectPath{}.child(ref(kind::Database, u"owl_elsewhere"_s)));
        QVERIFY(!other);
        QCOMPARE(other.error().category, DbError::Category::Unsupported);

        const Result<QList<ObjectNode>> schemas = cat().children(dbPath());
        QVERIFY(schemas);
        const ObjectNode *owl = byName(*schemas, u"owl_cat"_s);
        QVERIFY(owl);
        QCOMPARE(owl->badges.value(u"comment"_s).toString(), u"catalog fixture"_s);
        QVERIFY(byName(*schemas, u"pg_catalog"_s)->isSystem);
        QCOMPARE(schemas->last().ref, ref(kind::Group, group::Extensions));
        const Result<QList<ObjectNode>> extensions
            = cat().children(dbPath().child(schemas->last().ref));
        QVERIFY(names(*extensions).contains(u"plpgsql"_s));

        QCOMPARE(names(*cat().children(schemaPath())),
                 (QStringList{u"tables"_s, u"views"_s, u"matviews"_s, u"sequences"_s,
                              u"functions"_s, u"procedures"_s, u"types"_s}));
    }

    void schemaGroups()
    {
        const Result<QList<ObjectNode>> tables = cat().children(groupPath(group::Tables));
        QCOMPARE(names(*tables), (QStringList{u"customer"_s, u"events"_s, u"orders"_s}));
        QVERIFY(byName(*tables, u"events"_s)->badges.value(u"partitioned"_s).toBool());
        QCOMPARE(byName(*tables, u"customer"_s)->badges.value(u"comment"_s).toString(),
                 u"People"_s);

        QCOMPARE(names(*cat().children(groupPath(group::Views))), QStringList{u"big_orders"_s});
        QCOMPARE(names(*cat().children(groupPath(group::MaterializedViews))),
                 QStringList{u"order_totals"_s});
        QVERIFY(names(*cat().children(groupPath(group::Sequences))).contains(u"ticket"_s));

        const Result<QList<ObjectNode>> functions = cat().children(groupPath(group::Functions));
        QStringList shown;
        for (const ObjectNode &n : *functions)
            shown << n.displayName;
        QCOMPARE(shown,
                 (QStringList{u"add(a integer, b integer)"_s, u"add(a numeric, b numeric)"_s,
                              u"touch()"_s}));
        QVERIFY(functions->at(0).ref.oid != functions->at(1).ref.oid); // overloads
        QCOMPARE(names(*cat().children(groupPath(group::Procedures))), QStringList{u"noop"_s});

        const Result<QList<ObjectNode>> types = cat().children(groupPath(group::Types));
        QCOMPARE(names(*types), (QStringList{u"mood"_s, u"posint"_s}));
        QCOMPARE(types->at(0).badges.value(u"type"_s).toString(), u"enum"_s);
        QCOMPARE(types->at(1).badges.value(u"type"_s).toString(), u"domain"_s);
    }

    void relationGroups()
    {
        const ObjectPath customer = nodePath(group::Tables, u"customer"_s);
        QCOMPARE(names(*cat().children(customer)),
                 (QStringList{u"columns"_s, u"indexes"_s, u"constraints"_s, u"triggers"_s}));
        const Result<QList<ObjectNode>> columns
            = cat().children(customer.child(ref(kind::Group, group::Columns)));
        QCOMPARE(names(*columns), (QStringList{u"id"_s, u"email"_s, u"name"_s, u"mood"_s}));
        QCOMPARE(columns->first().icon, u"key"_s);
        QCOMPARE(columns->at(1).badges.value(u"comment"_s).toString(), u"Login"_s);
        QCOMPARE(names(*cat().children(customer.child(ref(kind::Group, group::Indexes)))),
                 (QStringList{u"customer_email_key"_s, u"customer_lower"_s, u"customer_pkey"_s}));
        const Result<QList<ObjectNode>> constraints
            = cat().children(customer.child(ref(kind::Group, group::Constraints)));
        QCOMPARE(names(*constraints).first(), u"customer_pkey"_s); // primary key first
        QVERIFY(names(*constraints).contains(u"customer_email_check"_s));
        QVERIFY(!names(*constraints).contains(u"customer_email_not_null"_s)); // PG 18 NOT NULL
        QCOMPARE(names(*cat().children(customer.child(ref(kind::Group, group::Triggers)))),
                 QStringList{u"customer_touch"_s});

        const ObjectPath view = nodePath(group::Views, u"big_orders"_s);
        QCOMPARE(names(*cat().children(view)), QStringList{u"columns"_s});
        const ObjectPath matview = nodePath(group::MaterializedViews, u"order_totals"_s);
        QCOMPARE(names(*cat().children(matview)), (QStringList{u"columns"_s, u"indexes"_s}));
    }

    void tableDefinitions()
    {
        const Result<TableDef> customer = cat().table(nodePath(group::Tables, u"customer"_s));
        QVERIFY2(customer, qPrintable(customer ? QString() : customer.error().toString()));
        QCOMPARE(customer->schema, u"owl_cat"_s);
        QCOMPARE(customer->comment, u"People"_s);
        QCOMPARE(customer->primaryKeyName, u"customer_pkey"_s);
        QCOMPARE(customer->primaryKey, QStringList{u"id"_s});
        const ColumnDef *id = customer->column(u"id");
        QVERIFY(id->autoIncrement);
        QCOMPARE(id->kind, ValueKind::Int);
        QCOMPARE(id->type, u"integer"_s);
        const ColumnDef *name = customer->column(u"name");
        QCOMPARE(name->type, u"character varying(50)"_s);
        QCOMPARE(name->collation, u"C"_s);
        QVERIFY(name->defaultExpr.startsWith(u"'anon'"_s));
        QCOMPARE(customer->column(u"email")->comment, u"Login"_s);
        QCOMPARE(customer->column(u"mood")->type, u"owl_cat.mood"_s);
        QCOMPARE(customer->uniques.size(), 1);
        QCOMPARE(customer->uniques.first().columns, QStringList{u"email"_s});
        QCOMPARE(customer->checks.size(), 1);
        QVERIFY(customer->checks.first().expression.contains(u"length(email) > 3"_s));
        QCOMPARE(customer->indexes.size(), 3);
        QCOMPARE(customer->triggers.size(), 1);
        QCOMPARE(customer->triggers.first().timing, u"BEFORE"_s);
        QCOMPARE(customer->triggers.first().events, QStringList{u"UPDATE"_s});

        const Result<TableDef> orders = cat().table(nodePath(group::Tables, u"orders"_s));
        QVERIFY(orders);
        QCOMPARE(orders->primaryKey, (QStringList{u"customer_id"_s, u"line"_s}));
        QVERIFY(orders->column(u"total")->generatedExpr.contains(u"amount"_s));
        QVERIFY(orders->column(u"total")->defaultExpr.isEmpty());
        QCOMPARE(orders->foreignKeys.size(), 1);
        const ForeignKeyDef &fk = orders->foreignKeys.first();
        QCOMPARE(fk.refSchema, u"owl_cat"_s);
        QCOMPARE(fk.refTable, u"customer"_s);
        QCOMPARE(fk.refColumns, QStringList{u"id"_s});
        QCOMPARE(fk.onDelete, u"CASCADE"_s);
        QCOMPARE(fk.onUpdate, u"NO ACTION"_s);
        const auto big = std::find_if(orders->indexes.cbegin(), orders->indexes.cend(),
                                      [](const IndexDef &i) { return i.name == u"orders_big"; });
        QVERIFY(big->predicate.contains(u"amount > "_s));
        QCOMPARE(big->method, u"btree"_s);
        QCOMPARE(big->columns, QStringList{u"amount"_s});

        // By name, without the OID the tree hands out.
        const Result<TableDef> view
            = cat().table(groupPath(group::Views).child(ref(kind::View, u"big_orders"_s)));
        QVERIFY(view);
        QCOMPARE(view->kind, QString(kind::View));
        QCOMPARE(view->columns.size(), 2);
        QVERIFY(cat()
                    .table(nodePath(group::Tables, u"events"_s))
                    ->options.value(u"partitioned"_s)
                    .toBool());
    }

    void ddlOfEveryKind()
    {
        const Result<QString> customer = cat().ddl(nodePath(group::Tables, u"customer"_s));
        QVERIFY2(customer, qPrintable(customer ? QString() : customer.error().toString()));
        for (const QString &part :
             {u"CREATE TABLE owl_cat.customer ("_s, u"GENERATED ALWAYS AS IDENTITY"_s,
              u"COLLATE \"C\""_s, u"CONSTRAINT customer_pkey PRIMARY KEY (id)"_s,
              u"CONSTRAINT customer_email_key UNIQUE (email)"_s,
              u"COMMENT ON TABLE owl_cat.customer IS 'People';"_s,
              u"COMMENT ON COLUMN owl_cat.customer.email IS 'Login';"_s,
              u"CREATE INDEX customer_lower ON owl_cat.customer"_s,
              u"CREATE TRIGGER customer_touch BEFORE UPDATE ON owl_cat.customer"_s})
            QVERIFY2(customer->contains(part), qPrintable(part + u"\n---\n"_s + *customer));
        QVERIFY(!customer->contains(u"NOT NULL email"_s));

        // The generated DDL is valid: run it again for a copy in another schema.
        exec(u"CREATE SCHEMA owl_cat2"_s);
        const QString copy
            = QString(*customer).replace(u"owl_cat.customer"_s, u"owl_cat2.customer"_s);
        for (const StatementSpan &s : m_driver.dialect().split(copy))
            exec(s.in(copy).toString());

        QVERIFY(cat().ddl(nodePath(group::Tables, u"orders"_s))->contains(u"STORED"_s));
        QVERIFY(cat()
                    .ddl(nodePath(group::Tables, u"events"_s))
                    ->contains(u"PARTITION BY RANGE (at)"_s));
        QVERIFY(cat()
                    .ddl(nodePath(group::Views, u"big_orders"_s))
                    ->startsWith(u"CREATE OR REPLACE VIEW owl_cat.big_orders AS"_s));
        const Result<QString> mv = cat().ddl(nodePath(group::MaterializedViews, u"order_totals"_s));
        QVERIFY(mv->startsWith(u"CREATE MATERIALIZED VIEW owl_cat.order_totals AS"_s));
        QVERIFY(mv->contains(u"CREATE INDEX order_totals_customer"_s));
        QVERIFY(cat()
                    .ddl(nodePath(group::Sequences, u"ticket"_s))
                    ->startsWith(u"CREATE SEQUENCE owl_cat.ticket AS bigint INCREMENT BY 1"_s));
        QVERIFY(cat()
                    .ddl(nodePath(group::Procedures, u"noop"_s))
                    ->contains(u"CREATE OR REPLACE PROCEDURE owl_cat.noop()"_s));
        const Result<QList<ObjectNode>> functions = cat().children(groupPath(group::Functions));
        QVERIFY(cat()
                    .ddl(groupPath(group::Functions).child(functions->at(1).ref))
                    ->contains(u"owl_cat.add(a numeric, b numeric)"_s));
        QCOMPARE(*cat().ddl(nodePath(group::Types, u"mood"_s)),
                 u"CREATE TYPE owl_cat.mood AS ENUM ('sad', 'happy');\n"_s);
        QVERIFY(cat()
                    .ddl(nodePath(group::Types, u"posint"_s))
                    ->contains(u"CREATE DOMAIN owl_cat.posint AS integer"_s));

        const ObjectPath customerPath = nodePath(group::Tables, u"customer"_s);
        const auto leaf = [&](QLatin1StringView group, const QString &name) {
            const ObjectPath g = customerPath.child(ref(kind::Group, group));
            const Result<QList<ObjectNode>> nodes = cat().children(g);
            const ObjectNode *n = nodes ? byName(*nodes, name) : nullptr;
            return n ? cat().ddl(g.child(n->ref)) : Result<QString>(fail(DbError{}));
        };
        QVERIFY(leaf(group::Indexes, u"customer_lower"_s)
                    ->startsWith(u"CREATE INDEX customer_lower"_s));
        QVERIFY(leaf(group::Triggers, u"customer_touch"_s)->startsWith(u"CREATE TRIGGER"_s));
        QCOMPARE(*leaf(group::Constraints, u"customer_email_check"_s),
                 u"ALTER TABLE owl_cat.customer ADD CONSTRAINT customer_email_check "
                 u"CHECK (length(email) > 3);"_s);

        const Result<QList<ObjectNode>> schemas = cat().children(dbPath());
        const ObjectNode *schema = byName(*schemas, u"owl_cat"_s);
        QVERIFY(cat().ddl(dbPath().child(schema->ref))->startsWith(u"CREATE SCHEMA owl_cat"_s));
        QVERIFY(!cat().ddl(customerPath.child(ref(kind::Group, group::Columns))));
    }
};

QTEST_GUILESS_MAIN(TestPgCatalog)
#include "tst_pg_catalog.moc"
