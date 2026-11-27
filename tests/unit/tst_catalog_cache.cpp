// SPDX-License-Identifier: GPL-3.0-or-later
// ObjectPath and CatalogCache over the fake driver: lazy loads on the worker thread, de-duplicated
// requests, errors, invalidation (also while a load runs), DDL clears (decisions 050, 162).
#include "fakedriver.h"
#include "owl/core/catalogcache.h"
#include "owl/core/connectionworker.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;

namespace {
ObjectPath path(std::initializer_list<std::pair<QLatin1StringView, QString>> parts)
{
    ObjectPath p;
    for (const auto &[k, n] : parts)
        p.parts.append({.kind = k, .name = n, .oid = {}});
    return p;
}
} // namespace

class TestCatalogCache : public QObject
{
    Q_OBJECT

    test::FakeDriver m_driver;
    std::unique_ptr<ConnectionWorker> m_worker;

private slots:
    void init()
    {
        m_worker = std::make_unique<ConnectionWorker>(&m_driver);
        QSignalSpy opened(m_worker.get(), &ConnectionWorker::opened);
        m_worker->open({});
        QVERIFY(opened.wait());
    }

    void cleanup() { m_worker.reset(); }

    void objectPaths()
    {
        const ObjectPath db = path({{kind::Database, u"app"_s}});
        const ObjectPath table
            = db.child({.kind = kind::Schema, .name = u"public"_s, .oid = {}})
                  .child({.kind = kind::Table, .name = u"users"_s, .oid = u"42"_s});
        QVERIFY(table.startsWith(db));
        QVERIFY(table.startsWith(ObjectPath{}));
        QVERIFY(!db.startsWith(table));
        QCOMPARE(table.find(kind::Schema), u"public"_s);
        QCOMPARE(table.find(kind::Index), QString());
        QCOMPARE(table.parent().last().name, u"public"_s);
        QCOMPARE(table.toString(), u"database:app/schema:public/table:users#42"_s);
        QCOMPARE(qHash(table), qHash(ObjectPath(table)));
        QCOMPARE(iconFor(kind::Constraint, u"foreign"), u"link"_s);
    }

    void loadsLazilyAndCaches()
    {
        CatalogCache cache(m_worker.get());
        QSignalSpy loaded(&cache, &CatalogCache::loaded);
        QVERIFY(!cache.children({}));
        cache.loadChildren({});
        cache.loadChildren({}); // de-duplicated while running
        QVERIFY(cache.isLoading(CatalogCache::What::Children, {}));
        QVERIFY(loaded.wait());
        QTest::qWait(50);
        QCOMPARE(loaded.size(), 1);
        const QList<ObjectNode> dbs = cache.children({}).value_or(QList<ObjectNode>{});
        QCOMPARE(dbs.size(), 1);
        QCOMPARE(dbs.first().ref.name, u"fake"_s);

        const ObjectPath tables
            = ObjectPath{}
                  .child(dbs.first().ref)
                  .child({.kind = kind::Group, .name = group::Tables, .oid = {}});
        cache.loadChildren(tables);
        QVERIFY(loaded.wait());
        const QList<ObjectNode> tableNodes = cache.children(tables).value_or(QList<ObjectNode>{});
        QCOMPARE(tableNodes.size(), 2);

        const ObjectPath a = tables.child(tableNodes.first().ref);
        cache.loadTable(a);
        cache.loadDdl(a);
        QTRY_VERIFY(cache.table(a) && cache.ddl(a));
        QCOMPARE(cache.table(a).value_or(TableDef{}).primaryKey, QStringList{u"id"_s});
        QVERIFY(cache.ddl(a).value_or(QString()).startsWith(u"CREATE TABLE a"_s));
    }

    void reportsErrors()
    {
        CatalogCache cache(m_worker.get());
        QSignalSpy failed(&cache, &CatalogCache::loadFailed);
        const ObjectPath bad = path({{kind::Database, u"fail"_s}});
        cache.loadChildren(bad);
        QVERIFY(failed.wait());
        QCOMPARE(failed.first().at(0).value<CatalogCache::What>(), CatalogCache::What::Children);
        QCOMPARE(failed.first().at(2).value<DbError>().category, DbError::Category::Permission);
        QVERIFY(!cache.children(bad));
        QVERIFY(!cache.isLoading(CatalogCache::What::Children, bad));
    }

    void invalidationDropsSubtreeAndRunningLoads()
    {
        CatalogCache cache(m_worker.get());
        QSignalSpy loaded(&cache, &CatalogCache::loaded);
        QSignalSpy invalidated(&cache, &CatalogCache::invalidated);
        const ObjectPath db = path({{kind::Database, u"fake"_s}});
        const ObjectPath tables = db.child({.kind = kind::Group, .name = group::Tables, .oid = {}});
        cache.loadChildren({});
        cache.loadChildren(db);
        cache.loadChildren(tables);
        QTRY_COMPARE(loaded.size(), 3);

        cache.invalidate(db);
        QCOMPARE(invalidated.size(), 1);
        QVERIFY(cache.children({})); // above the invalidated node: kept
        QVERIFY(!cache.children(db));
        QVERIFY(!cache.children(tables));

        // A load that is invalidated while it runs is dropped.
        const ObjectPath slow = path({{kind::Database, u"slow"_s}});
        cache.loadChildren(slow);
        QTest::qWait(50);
        cache.invalidate(slow);
        QTest::qWait(500);
        QVERIFY(!cache.children(slow));
        QCOMPARE(loaded.size(), 3);
    }

    void ddlClearsEverything()
    {
        CatalogCache cache(m_worker.get());
        QSignalSpy invalidated(&cache, &CatalogCache::invalidated);
        cache.loadChildren({});
        QTRY_VERIFY(cache.children({}));
        cache.noteExecuted(StatementClass::Write);
        QVERIFY(cache.children({}));
        cache.noteExecuted(StatementClass::Ddl);
        QVERIFY(!cache.children({}));
        QCOMPARE(invalidated.size(), 1);
        QVERIFY(invalidated.first().at(0).value<ObjectPath>().isRoot());
    }

    void notConnected()
    {
        ConnectionWorker closed(&m_driver); // never opened
        CatalogCache cache(&closed);
        QSignalSpy failed(&cache, &CatalogCache::loadFailed);
        cache.loadChildren({});
        QVERIFY(failed.wait());
        QCOMPARE(failed.first().at(2).value<DbError>().category, DbError::Category::Connection);
    }

    void cacheDeletedWhileLoading()
    {
        auto cache = std::make_unique<CatalogCache>(m_worker.get());
        cache->loadChildren(path({{kind::Database, u"slow"_s}}));
        cache.reset();
        QTest::qWait(500); // the answer arrives and is dropped without touching the cache
    }
};

QTEST_GUILESS_MAIN(TestCatalogCache)
#include "tst_catalog_cache.moc"
