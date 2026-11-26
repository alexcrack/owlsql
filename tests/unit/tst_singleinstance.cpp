// SPDX-License-Identifier: GPL-3.0-or-later
#include "singleinstance.h"

#include <QSignalSpy>
#include <QTest>
#include <QThread>
#include <QUuid>

using namespace Qt::StringLiterals;
using owl::app::SingleInstance;

class TestSingleInstance : public QObject
{
    Q_OBJECT

private slots:
    void serverNameIsStablePerDataDir()
    {
        const QString a = SingleInstance::serverNameFor(u"/tmp/a"_s);
        QCOMPARE(SingleInstance::serverNameFor(u"/tmp/a/"_s), a); // normalised path
        QVERIFY(SingleInstance::serverNameFor(u"/tmp/b"_s) != a);
        QVERIFY(a.startsWith(u"owlsql-"_s));
    }

    void secondInstanceForwardsArguments()
    {
        const QString name
            = u"owlsql-test-"_s + QUuid::createUuid().toString(QUuid::Id128).left(12);

        SingleInstance primary(name, nullptr);
        QVERIFY(primary.start({}));
        QSignalSpy spy(&primary, &SingleInstance::activationRequested);

        // The secondary runs on a helper thread: its blocking hand-off needs the primary's
        // event loop (this thread) to accept the connection.
        bool secondaryIsPrimary = true;
        QThread *thread = QThread::create([&] {
            SingleInstance secondary(name, nullptr);
            secondaryIsPrimary = secondary.start({u"query.sql"_s, u"postgres://db/app"_s});
        });
        thread->start();
        QTRY_VERIFY_WITH_TIMEOUT(thread->isFinished(), 5000);
        delete thread;

        QVERIFY(!secondaryIsPrimary);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 5000);
        QCOMPARE(spy.at(0).at(0).toStringList(),
                 (QStringList{u"query.sql"_s, u"postgres://db/app"_s}));
    }
};

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "tst_singleinstance.moc"
