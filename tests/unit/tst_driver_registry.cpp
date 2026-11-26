// SPDX-License-Identifier: GPL-3.0-or-later
// DriverRegistry: real plugin files built from the fake driver (decision 049).
#include "fakedriver.h"
#include "owl/core/connectionworker.h"
#include "owl/core/driverregistry.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestDriverRegistry : public QObject
{
    Q_OBJECT

private slots:
    void addsDriversDirectly()
    {
        owl::DriverRegistry registry;
        QVERIFY(registry.add(std::make_shared<owl::test::FakeDriver>()));
        QVERIFY(!registry.add(std::make_shared<owl::test::FakeDriver>())); // same id
        QVERIFY(!registry.add(nullptr));
        QCOMPARE(registry.ids(), QStringList{u"fake"_s});
        QVERIFY(registry.driver(u"fake"_s));
        QVERIFY(!registry.driver(u"postgres"_s));
    }

    void loadsMatchingPluginsAndRejectsOthers()
    {
        owl::DriverRegistry registry;
        const QStringList problems = registry.loadPlugins(QStringLiteral(OWL_TEST_DRIVER_DIR));
        QCOMPARE(registry.ids(), QStringList{u"fake"_s});
        QCOMPARE(problems.size(), 1);
        QVERIFY2(problems.first().contains(u"core API 999"_s), qPrintable(problems.first()));

        // The loaded plugin is a working driver.
        owl::IDriver *driver = registry.driver(u"fake"_s);
        QVERIFY(driver);
        QCOMPARE(driver->displayName(), u"Fake"_s);
        owl::ConnectionWorker worker(driver);
        QSignalSpy opened(&worker, &owl::ConnectionWorker::opened);
        worker.open({});
        QVERIFY(opened.wait(5000));
        owl::Statement statement;
        statement.sql = u"rows 3"_s;
        owl::QueryJob *job = worker.execute(statement);
        QTRY_COMPARE(job->state(), owl::QueryJob::State::Finished);
        QCOMPARE(job->rowsFetched(), 3);
    }

    void missingDirectoryIsHarmless()
    {
        owl::DriverRegistry registry;
        QVERIFY(registry.loadPlugins(u"/does/not/exist"_s).isEmpty());
        QVERIFY(registry.ids().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestDriverRegistry)
#include "tst_driver_registry.moc"
