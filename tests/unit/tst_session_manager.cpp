// SPDX-License-Identifier: GPL-3.0-or-later
// SessionManager (the Welcome tab's backend, M1.9b): drafts ⇄ sessions with password modes,
// validation, URL drafts, the list model, import preview / apply, export, Test, open / close.
#include "appdatabase.h"
#include "fakedriver.h"
#include "owl/core/driverregistry.h"
#include "pgdriver.h"
#include "secretstore.h"
#include "sessionlistmodel.h"
#include "sessionmanager.h"
#include "sessionstore.h"
#include "sqlitedriver.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;
using namespace owl::app;

class TestSessionManager : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    std::unique_ptr<AppDatabase> m_db;
    std::unique_ptr<SessionStore> m_store;
    DriverRegistry m_drivers;
    MemorySecretStore m_secrets;
    std::unique_ptr<SessionManager> m_manager;

    QVariantMap draftFor(const QString &driver, const QString &name, const QVariantMap &values)
    {
        QVariantMap d = m_manager->newDraft(driver);
        d.insert(u"name"_s, name);
        QVariantMap v = d.value(u"values"_s).toMap();
        v.insert(values);
        d.insert(u"values"_s, v);
        return d;
    }

    void readStored(const QString &id, QString &value)
    {
        value = u"<none>"_s;
        bool done = false;
        m_secrets.read(id, u"password"_s, [&](const SecretStore::Outcome &o) {
            value = o.ok ? o.value : u"<none>"_s;
            done = true;
        });
        QVERIFY(QTest::qWaitFor([&] { return done; }));
    }

    SessionInfo stored(const QString &id) const
    {
        return m_store->session(id).value_or(SessionInfo{});
    }

    QString storedPassword(const QString &id)
    {
        QString value;
        readStored(id, value);
        return value;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_drivers.add(std::make_shared<sqlite::SqliteDriver>()));
        QVERIFY(m_drivers.add(std::make_shared<postgres::PgDriver>()));
        QVERIFY(m_drivers.add(std::make_shared<test::FakeDriver>()));
    }

    void init()
    {
        static int n = 0;
        m_db = std::make_unique<AppDatabase>(u"manager-%1"_s.arg(++n));
        QString error;
        QVERIFY(m_db->open(m_dir.filePath(u"m%1.db"_s.arg(n)), m_dir.filePath(u"b"_s), &error));
        m_store = std::make_unique<SessionStore>(m_db->database());
        m_manager = std::make_unique<SessionManager>(m_store.get(), &m_drivers, &m_secrets);
    }

    void cleanup()
    {
        m_manager.reset();
        m_store.reset();
        m_db.reset();
    }

    void draftsRoundTripWithPasswords()
    {
        QCOMPARE(m_manager->drivers().first().toMap().value(u"id"_s).toString(), u"postgres"_s);
        const QVariantList form = m_manager->form(u"fake"_s);
        QCOMPARE(form.size(), 2);
        QCOMPARE(form.at(1).toMap().value(u"type"_s).toString(), u"integer"_s);

        QVariantMap d = draftFor(u"fake"_s, u"dev box"_s, {{u"host"_s, u"db.local"_s}});
        d.insert(u"level"_s, u"staging"_s);
        d.insert(u"startupScript"_s, u"update 1"_s);
        // The fake form has no password field: the mode is "none".
        const QVariantMap r = m_manager->save(d);
        QVERIFY2(r.value(u"ok"_s).toBool(), qPrintable(r.value(u"error"_s).toString()));
        const QString id = r.value(u"id"_s).toString();
        const QVariantMap back = m_manager->draft(id);
        QCOMPARE(back.value(u"name"_s).toString(), u"dev box"_s);
        QCOMPARE(back.value(u"level"_s).toString(), u"staging"_s);
        QCOMPARE(back.value(u"startupScript"_s).toString(), u"update 1"_s);
        QCOMPARE(back.value(u"values"_s).toMap().value(u"host"_s).toString(), u"db.local"_s);
        QCOMPARE(stored(id).secretMode(secret::Password), SecretMode::None);
        QCOMPARE(m_manager->model()->rowCount(), 1);
    }

    void validation()
    {
        QVariantMap d = m_manager->newDraft(u"fake"_s);
        const QVariantMap errors = m_manager->validate(d);
        QVERIFY(errors.contains(u"name"_s));
        QVERIFY(errors.contains(u"host"_s));
        QVERIFY(!m_manager->save(d).value(u"ok"_s).toBool());
        QCOMPARE(m_store->sessions().size(), 0);

        d = draftFor(u"fake"_s, u"x"_s, {{u"host"_s, u"h"_s}, {u"option.speed"_s, 99}});
        QVERIFY(m_manager->validate(d).contains(u"option.speed"_s));
    }

    void urlDraft()
    {
        const QVariantMap d = m_manager->draftFromUrl(u"fake://me:pw@db.local/app"_s);
        QCOMPARE(d.value(u"driverId"_s).toString(), u"fake"_s);
        QCOMPARE(d.value(u"name"_s).toString(), u"me@db.local/app"_s);
        QCOMPARE(d.value(u"values"_s).toMap().value(u"host"_s).toString(), u"db.local"_s);
        QVERIFY(m_manager->draftFromUrl(u"nonsense"_s).contains(u"error"_s));
    }

    void foldersAndModel()
    {
        const QString work = m_manager->addFolder(u"Work"_s);
        QVERIFY(!work.isEmpty());
        QVariantMap d = draftFor(u"fake"_s, u"inside"_s, {{u"host"_s, u"h"_s}});
        d.insert(u"folderId"_s, work);
        const QString inside = m_manager->save(d).value(u"id"_s).toString();
        m_manager->save(draftFor(u"fake"_s, u"outside"_s, {{u"host"_s, u"h"_s}}));

        SessionListModel *model = m_manager->model();
        QCOMPARE(model->rowCount(), 3); // Work, inside, outside
        QCOMPARE(model->data(model->index(0), SessionListModel::KindRole).toString(), u"folder"_s);
        QCOMPARE(model->data(model->index(0), SessionListModel::ChildCountRole).toInt(), 1);
        QCOMPARE(model->data(model->index(1), SessionListModel::DepthRole).toInt(), 1);
        QCOMPARE(model->data(model->index(1), SessionListModel::TagRole).toString(), u"green"_s);

        m_manager->setFolderExpanded(work, false);
        QCOMPARE(model->rowCount(), 2);
        QCOMPARE(model->rowOf(inside), -1);

        model->setFilter(u"insi"_s);
        QCOMPARE(model->rowCount(), 1);
        QVERIFY(model->data(model->index(0), SessionListModel::SubtitleRole)
                    .toString()
                    .startsWith(u"Work · "_s));
        model->setFilter({});

        QVERIFY(m_manager->renameFolder(work, u"Job"_s));
        QCOMPARE(m_manager->folders().first().toMap().value(u"name"_s).toString(), u"Job"_s);
        QVERIFY(m_manager->removeFolder(work));
        QCOMPARE(model->rowCount(), 2);
    }

    void importAndExport()
    {
        m_manager->save(draftFor(u"fake"_s, u"one"_s, {{u"host"_s, u"h"_s}}));
        const QString file = m_dir.filePath(u"out.owlsql-sessions.json"_s);
        const QVariantMap exported = m_manager->exportTo(QUrl::fromLocalFile(file));
        QCOMPARE(exported.value(u"exported"_s).toInt(), 1);

        const QVariantMap preview = m_manager->inspectImport(QUrl::fromLocalFile(file));
        QVERIFY(preview.value(u"ok"_s).toBool());
        QCOMPARE(preview.value(u"source"_s).toString(), u"json"_s);
        QCOMPARE(preview.value(u"sessions"_s).toInt(), 1);
        QCOMPARE(preview.value(u"conflicts"_s).toInt(), 1);
        QCOMPARE(m_manager->applyImport(u"keepBoth"_s).value(u"imported"_s).toInt(), 1);
        QCOMPARE(m_store->sessions().size(), 2);
        QVERIFY(!m_manager->applyImport(u"keepBoth"_s).value(u"ok"_s).toBool()); // consumed

        QFile pgpass(m_dir.filePath(u"pgpass"_s));
        QVERIFY(pgpass.open(QIODevice::WriteOnly));
        pgpass.write("db:5432:app:me:pw\n");
        pgpass.close();
        const QVariantMap p = m_manager->inspectImport(QUrl::fromLocalFile(pgpass.fileName()));
        QCOMPARE(p.value(u"source"_s).toString(), u"pgpass"_s);
        QVERIFY(m_manager->applyImport(u"skip"_s).value(u"ok"_s).toBool());
        QCOMPARE(m_store->sessions().size(), 3);

        QVERIFY(!m_manager->inspectImport(QUrl::fromLocalFile(m_dir.filePath(u"missing"_s)))
                     .value(u"ok"_s)
                     .toBool());
    }

    void testConnection()
    {
        QSignalSpy finished(m_manager.get(), &SessionManager::testFinished);
        m_manager->test(draftFor(u"sqlite"_s, u"mem"_s, {{u"database"_s, u":memory:"_s}}));
        QVERIFY(m_manager->isTesting());
        QVERIFY(finished.wait());
        QVERIFY(finished.first().at(0).toBool());
        QVERIFY(finished.first().at(1).toString().contains(u"SQLite"_s));
        QVERIFY(!m_manager->isTesting());

        m_manager->test(draftFor(u"fake"_s, u"bad"_s, {{u"host"_s, u"fail"_s}}));
        QVERIFY(finished.wait());
        QVERIFY(!finished.last().at(0).toBool());
    }

    void openAndClose()
    {
        QVariantMap d = draftFor(u"sqlite"_s, u"mem"_s, {{u"database"_s, u":memory:"_s}});
        d.insert(u"level"_s, u"prod"_s);
        const QString id = m_manager->save(d).value(u"id"_s).toString();
        QSignalSpy states(m_manager.get(), &SessionManager::sessionStateChanged);
        m_manager->open(id);
        QCOMPARE(m_manager->openSessions(), QStringList{id});
        QCOMPARE(m_manager->activeSession(), id);
        QTRY_COMPARE(m_manager->state(id), u"connected"_s);
        const QVariantMap info = m_manager->activeInfo();
        QCOMPARE(info.value(u"level"_s).toString(), u"prod"_s);
        QCOMPARE(info.value(u"tag"_s).toString(), u"red"_s);
        QCOMPARE(info.value(u"product"_s).toString(), u"SQLite"_s);
        QVERIFY(stored(id).lastUsedAt.isValid());
        QVERIFY(!states.isEmpty());

        m_manager->close(id);
        QVERIFY(m_manager->openSessions().isEmpty());
        QVERIFY(m_manager->activeSession().isEmpty());
        QVERIFY(m_manager->state(id).isEmpty());

        // A failing open reports why.
        const QString broken
            = m_manager->save(draftFor(u"fake"_s, u"refused"_s, {{u"host"_s, u"auth"_s}}))
                  .value(u"id"_s)
                  .toString();
        QSignalSpy failed(m_manager.get(), &SessionManager::sessionFailed);
        m_manager->open(broken);
        QVERIFY(failed.wait());
        QCOMPARE(m_manager->state(broken), u"failed"_s);
        QVERIFY(m_manager->info(broken).value(u"error"_s).toString().contains(u"password"_s));

        QVERIFY(m_manager->remove(broken)); // removing an open session closes it
        QVERIFY(m_manager->openSessions().isEmpty());
    }

    void passwordsGoToTheKeychain()
    {
        QVariantMap d
            = draftFor(u"postgres"_s, u"pg"_s,
                       {{u"host"_s, u"db"_s}, {u"user"_s, u"me"_s}, {u"password"_s, u"s3cret"_s}});
        QVERIFY(d.value(u"savePassword"_s).toBool()); // a keychain is available
        const QString id = m_manager->save(d).value(u"id"_s).toString();
        QVERIFY(!id.isEmpty());
        QCOMPARE(storedPassword(id), u"s3cret"_s);
        QVERIFY(stored(id).params.password.isEmpty());
        QVariantMap back = m_manager->draft(id);
        QVERIFY(back.value(u"hasStoredPassword"_s).toBool());
        QVERIFY(back.value(u"values"_s).toMap().value(u"password"_s).toString().isEmpty());

        // Saving with the field left blank keeps the stored password.
        QVERIFY(m_manager->save(back).value(u"ok"_s).toBool());
        QCOMPARE(storedPassword(id), u"s3cret"_s);

        // Not saving passwords: asked on connect, and the stored one is removed.
        back.insert(u"savePassword"_s, false);
        QVERIFY(m_manager->save(back).value(u"ok"_s).toBool());
        QCOMPARE(stored(id).secretMode(secret::Password), SecretMode::Ask);
        QCOMPARE(storedPassword(id), u"<none>"_s);
    }
};

QTEST_GUILESS_MAIN(TestSessionManager)
#include "tst_session_manager.moc"
