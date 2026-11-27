// SPDX-License-Identifier: GPL-3.0-or-later
// Saved sessions (M1.9a): SessionStore over owlsql.db, the secret stores, import / export
// (JSON, URL, .pgpass, .my.cnf, conflict policies) and the runtime Session over the fake driver
// (secret modes, startup script, failures, per-database connections).
#include "appdatabase.h"
#include "fakedriver.h"
#include "owl/core/driverregistry.h"
#include "secretstore.h"
#include "session.h"
#include "sessionimport.h"
#include "sessionstore.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;
using namespace owl::app;

namespace {

SessionInfo sample(const QString &name, const QString &driver = u"fake"_s)
{
    SessionInfo s;
    s.name = name;
    s.driverId = driver;
    s.params.host = u"db.local"_s;
    s.params.user = u"me"_s;
    s.params.password = u"must-not-be-stored"_s;
    s.safetyLevel = SafetyLevel::Prod;
    s.color = u"red"_s;
    s.options.startupScript = u"SET search_path TO app"_s;
    s.options.visibleDatabases = {u"app"_s};
    s.options.fetchSize = 500;
    s.notes = u"VPN required"_s;
    s.secretModes.insert(secret::Password, SecretMode::Keychain);
    return s;
}

} // namespace

class TestSessions : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    std::unique_ptr<AppDatabase> m_db;
    std::unique_ptr<SessionStore> m_store;
    test::FakeDriver m_driver;
    MemorySecretStore m_secrets;

    // A stored session, or an empty one (id "") when there is none.
    SessionInfo stored(const QString &id) const
    {
        return m_store->session(id).value_or(SessionInfo{});
    }

private slots:
    void init()
    {
        static int n = 0;
        m_db = std::make_unique<AppDatabase>(u"sessions-%1"_s.arg(++n));
        QString error;
        QVERIFY2(m_db->open(m_dir.filePath(u"owlsql-%1.db"_s.arg(n)), m_dir.filePath(u"backups"_s),
                            &error),
                 qPrintable(error));
        m_store = std::make_unique<SessionStore>(m_db->database());
    }

    void cleanup()
    {
        m_store.reset();
        m_db.reset();
    }

    // --- SessionStore ---------------------------------------------------------------------------

    void savesAndReadsSessions()
    {
        QSignalSpy changed(m_store.get(), &SessionStore::changed);
        const Result<SessionInfo> saved = m_store->saveSession(sample(u"prod-pg"_s));
        QVERIFY2(saved, qPrintable(saved ? QString() : saved.error().message));
        QVERIFY(!saved->id.isEmpty());
        QVERIFY(saved->createdAt.isValid());
        QVERIFY(saved->params.password.isEmpty()); // secrets never reach owlsql.db
        QCOMPARE(changed.size(), 1);

        const SessionInfo read = stored(saved->id);
        QCOMPARE(read, *saved);
        QCOMPARE(read.safetyLevel, SafetyLevel::Prod);
        QCOMPARE(read.options.fetchSize, 500);
        QCOMPARE(read.options.visibleDatabases, QStringList{u"app"_s});
        QCOMPARE(read.secretMode(secret::Password), SecretMode::Keychain);
        QCOMPARE(read.secretMode(secret::SshPassword), SecretMode::None);

        SessionInfo edited = read;
        edited.name = u"prod-pg-2"_s;
        const Result<SessionInfo> updated = m_store->saveSession(edited);
        QCOMPARE(updated->createdAt, saved->createdAt);
        QCOMPARE(m_store->sessions().size(), 1);
        QCOMPARE(m_store->sessions().first().name, u"prod-pg-2"_s);

        QVERIFY(m_store->markUsed(saved->id));
        QVERIFY(stored(saved->id).lastUsedAt.isValid());
        QVERIFY(m_store->removeSession(saved->id));
        QVERIFY(m_store->sessions().isEmpty());
    }

    void foldersAndOrdering()
    {
        const Result<FolderInfo> work = m_store->addFolder(u"Work"_s);
        const Result<FolderInfo> clients = m_store->addFolder(u"Clients"_s, work->id);
        QVERIFY(work && clients);
        QCOMPARE(clients->parentId, work->id);

        SessionInfo a = sample(u"a"_s);
        a.folderId = clients->id;
        SessionInfo b = sample(u"b"_s);
        b.folderId = clients->id;
        const QString aId = m_store->saveSession(a)->id;
        const QString bId = m_store->saveSession(b)->id;
        QCOMPARE(stored(bId).position, 1); // appended
        QVERIFY(m_store->moveSession(bId, clients->id, 0));
        QCOMPARE(stored(bId).position, 0);
        QCOMPARE(stored(aId).position, 1);

        // Removing a folder removes its subfolders; their sessions move to the top level.
        QVERIFY(m_store->removeFolder(work->id));
        QVERIFY(m_store->folders().isEmpty());
        QCOMPARE(m_store->sessions().size(), 2);
        QVERIFY(stored(aId).folderId.isEmpty());
    }

    void duplicateDoesNotCopySecrets()
    {
        const QString id = m_store->saveSession(sample(u"prod"_s))->id;
        const Result<SessionInfo> copy = m_store->duplicateSession(id);
        QVERIFY(copy);
        QVERIFY(copy->id != id);
        QCOMPARE(copy->name, u"prod (copy)"_s);
        QCOMPARE(copy->secretMode(secret::Password), SecretMode::Ask);
        QCOMPARE(copy->options, stored(id).options);
    }

    // --- Import / export ------------------------------------------------------------------------

    void jsonRoundTrip()
    {
        const Result<FolderInfo> team = m_store->addFolder(u"Team"_s);
        SessionInfo s = sample(u"shared"_s, u"postgres"_s);
        s.folderId = team->id;
        s.params.ssl.caFile = u"/definitely/missing/ca.pem"_s;
        const SessionInfo saved = *m_store->saveSession(s);

        const QJsonDocument doc = exportSessions(m_store->folders(), m_store->sessions());
        const QByteArray bytes = doc.toJson();
        QVERIFY(!bytes.contains("must-not-be-stored"));
        QVERIFY(bytes.contains("\"format\": \"owlsql-sessions\""));

        const Result<ImportBundle> bundle = importSessionsJson(bytes);
        QVERIFY2(bundle, qPrintable(bundle ? QString() : bundle.error().message));
        QCOMPARE(bundle->folders.size(), 1);
        QCOMPARE(bundle->sessions.size(), 1);
        const SessionInfo &in = bundle->sessions.first().info;
        QCOMPARE(in.params, saved.params);
        QCOMPARE(in.options, saved.options);
        QCOMPARE(in.safetyLevel, SafetyLevel::Prod);
        QCOMPARE(in.secretMode(secret::Password), SecretMode::Ask); // the secret stayed behind
        QCOMPARE(bundle->warnings.size(), 1); // the CA file does not exist here

        QVERIFY(!importSessionsJson("{\"format\": \"something\"}"));
        QVERIFY(!importSessionsJson("{\"format\": \"owlsql-sessions\", \"version\": 99}"));
        QVERIFY(!importSessionsJson("not json"));
    }

    void conflictPolicies()
    {
        const SessionInfo existing = *m_store->saveSession(sample(u"one"_s));
        ImportBundle bundle;
        ImportedSession incoming;
        incoming.info = existing;
        incoming.info.name = u"one (theirs)"_s;
        bundle.sessions = {incoming};

        QCOMPARE(applyImport(*m_store, &m_secrets, bundle, ConflictPolicy::Skip)->size(), 0);
        QCOMPARE(m_store->sessions().size(), 1);
        QVERIFY(applyImport(*m_store, &m_secrets, bundle, ConflictPolicy::Replace));
        QCOMPARE(m_store->sessions().size(), 1);
        QCOMPARE(stored(existing.id).name, u"one (theirs)"_s);
        QVERIFY(applyImport(*m_store, &m_secrets, bundle, ConflictPolicy::KeepBoth));
        QCOMPARE(m_store->sessions().size(), 2);
    }

    void urlImportStoresThePassword()
    {
        DriverRegistry drivers;
        QVERIFY(drivers.add(std::make_shared<test::FakeDriver>()));
        const Result<ImportBundle> bundle = importFromUrl(drivers, u"fake://me:pw@db.local/app"_s);
        QVERIFY(bundle);
        QCOMPARE(bundle->sessions.first().info.name, u"me@db.local/app"_s);
        QCOMPARE(bundle->sessions.first().password, u"pw"_s);

        const Result<QList<SessionInfo>> written = applyImport(*m_store, &m_secrets, *bundle);
        QCOMPARE(written->size(), 1);
        const SessionInfo s = written->first();
        QCOMPARE(s.importedFrom, u"url"_s);
        QCOMPARE(s.secretMode(secret::Password), SecretMode::Keychain);
        QVERIFY(stored(s.id).params.password.isEmpty());
        QString stored;
        m_secrets.read(s.id, secret::Password,
                       [&](const SecretStore::Outcome &o) { stored = o.value; });
        QTRY_COMPARE(stored, u"pw"_s);

        QVERIFY(!importFromUrl(drivers, u"gopher://nothing"_s));
    }

    void withoutKeychainPasswordsAreAsked()
    {
        MemorySecretStore none;
        none.setAvailable(false);
        ImportBundle bundle = importPgpass(u"db:5432:app:me:pw\n"_s);
        const Result<QList<SessionInfo>> written = applyImport(*m_store, &none, bundle);
        QCOMPARE(written->first().secretMode(secret::Password), SecretMode::Ask);
        QCOMPARE(none.size(), 0);
    }

    void pgpass()
    {
        const ImportBundle b = importPgpass(u"# comment\n"
                                            u"db.example.com:5432:app:alice:s3cr\\:et\n"
                                            u"/var/run/postgresql:*:*:bob:pw\n"
                                            u"*:5432:*:carol:pw\n"
                                            u"broken line\n"_s);
        QCOMPARE(b.sessions.size(), 2);
        QCOMPARE(b.warnings.size(), 2); // wildcard host, malformed line
        const ImportedSession &alice = b.sessions.at(0);
        QCOMPARE(alice.info.driverId, u"postgres"_s);
        QCOMPARE(alice.info.params.host, u"db.example.com"_s);
        QCOMPARE(alice.info.params.port, 5432);
        QCOMPARE(alice.password, u"s3cr:et"_s);
        QCOMPARE(alice.info.name, u"alice@db.example.com:5432/app"_s);
        const ImportedSession &bob = b.sessions.at(1);
        QCOMPARE(bob.info.params.transport, ConnectionParams::Transport::Socket);
        QCOMPARE(bob.info.params.port, 0);
        QVERIFY(bob.info.params.database.isEmpty());
    }

    void myCnf()
    {
        const ImportBundle b = importMyCnf(u"[client]\n"
                                           u"user = app\n"
                                           u"password = \"p w\"\n"
                                           u"port=3307\n"
                                           u"[mysql]\n"
                                           u"host = db.local\n"
                                           u"database = shop\n"
                                           u"[clientstaging]\n"
                                           u"host = staging.local\n"
                                           u"ssl_mode = REQUIRED\n"
                                           u"[mysqldump]\n"
                                           u"quick\n"
                                           u"!includedir /etc/mysql/conf.d/\n"_s);
        QCOMPARE(b.sessions.size(), 2);
        QCOMPARE(b.warnings.size(), 1); // the include
        const SessionInfo main = b.sessions.at(0).info;
        QCOMPARE(main.driverId, u"mysql"_s);
        QCOMPARE(main.params.host, u"db.local"_s);
        QCOMPARE(main.params.port, 3307);
        QCOMPARE(main.params.database, u"shop"_s);
        QCOMPARE(b.sessions.at(0).password, u"p w"_s);
        QCOMPARE(main.name, u"app@db.local:3307/shop (client)"_s);
        const SessionInfo staging = b.sessions.at(1).info;
        QCOMPARE(staging.params.host, u"staging.local"_s);
        QCOMPARE(staging.params.user, u"app"_s); // from [client]
        QCOMPARE(staging.params.ssl.mode, SslOptions::Mode::Require);
    }

    // --- Runtime Session ------------------------------------------------------------------------

    void connectsWithAStoredPassword()
    {
        SessionInfo info = *m_store->saveSession(sample(u"keychain"_s));
        info.options.startupScript.clear();
        m_secrets.write(info.id, secret::Password, u"pw"_s);
        Session session(info, &m_driver, &m_secrets);
        QSignalSpy connected(&session, &Session::connected);
        session.connectToServer();
        QCOMPARE(session.state(), Session::State::Connecting);
        QVERIFY(connected.wait());
        QCOMPARE(session.state(), Session::State::Connected);
        QCOMPARE(session.serverInfo().product, u"Fake"_s);
        QVERIFY(session.catalog());
        // The fake engine has no per-database connections: every database uses meta.
        QVERIFY(!session.usesDatabaseConnections());
        QCOMPARE(session.catalogFor(u"other"_s), session.catalog());
        QCOMPARE(session.workerFor(u"other"_s), session.metaWorker());

        QSignalSpy loaded(session.catalog(), &CatalogCache::loaded);
        session.catalog()->loadChildren({});
        QVERIFY(loaded.wait());

        QSignalSpy disconnected(&session, &Session::disconnected);
        session.disconnectFromServer();
        QCOMPARE(disconnected.size(), 1);
        QVERIFY(!session.catalog());
        QVERIFY(!session.catalogFor(u"x"_s));
    }

    void asksForMissingPasswords()
    {
        SessionInfo info = sample(u"ask"_s);
        info.id = newId();
        info.options.startupScript.clear();
        info.secretModes.insert(secret::Password, SecretMode::Keychain); // but nothing stored
        Session session(info, &m_driver, &m_secrets);
        QSignalSpy asked(&session, &Session::passwordRequired);
        QSignalSpy changedInfo(&session, &Session::infoChanged);
        session.connectToServer();
        QVERIFY(asked.wait());
        QCOMPARE(session.state(), Session::State::NeedsPassword);

        QSignalSpy connected(&session, &Session::connected);
        session.providePassword(u"typed"_s, /*remember=*/true);
        QVERIFY(connected.wait());
        QCOMPARE(changedInfo.size(), 1);
        QCOMPARE(session.info().secretMode(secret::Password), SecretMode::Keychain);
        QString stored;
        m_secrets.read(info.id, secret::Password,
                       [&](const SecretStore::Outcome &o) { stored = o.value; });
        QTRY_COMPARE(stored, u"typed"_s);
    }

    void askModeAndNoneMode()
    {
        SessionInfo info = sample(u"modes"_s);
        info.id = newId();
        info.options.startupScript.clear();
        info.secretModes.insert(secret::Password, SecretMode::Ask);
        Session ask(info, &m_driver, &m_secrets);
        QSignalSpy asked(&ask, &Session::passwordRequired);
        ask.connectToServer();
        QCOMPARE(asked.size(), 1); // no keychain round trip

        info.secretModes.insert(secret::Password, SecretMode::None);
        Session none(info, &m_driver, &m_secrets);
        QSignalSpy connected(&none, &Session::connected);
        none.connectToServer();
        QVERIFY(connected.wait());
    }

    void startupScriptAndFailures()
    {
        SessionInfo info = sample(u"startup"_s);
        info.id = newId();
        info.secretModes.insert(secret::Password, SecretMode::None);
        info.options.startupScript = u"update 1; notice hello;"_s;
        Session ok(info, &m_driver, &m_secrets);
        QCOMPARE(ok.startupStatements(), (QStringList{u"update 1"_s, u"notice hello"_s}));
        QSignalSpy connected(&ok, &Session::connected);
        ok.connectToServer();
        QVERIFY(connected.wait());

        info.options.startupScript = u"update 1; error 3"_s;
        Session broken(info, &m_driver, &m_secrets);
        QSignalSpy failed(&broken, &Session::failed);
        broken.connectToServer();
        QVERIFY(failed.wait());
        QCOMPARE(broken.state(), Session::State::Failed);
        QVERIFY(broken.lastError().message.startsWith(u"Startup script failed"_s));
        QVERIFY(!broken.metaWorker());

        info.options.startupScript.clear();
        info.params.host = u"auth"_s; // the fake server refuses the login
        Session refused(info, &m_driver, &m_secrets);
        QSignalSpy refusedSpy(&refused, &Session::failed);
        refused.connectToServer();
        QVERIFY(refusedSpy.wait());
        QCOMPARE(refused.lastError().category, DbError::Category::Auth);
        // Connecting again after a failure is allowed.
        info.params.host = u"db.local"_s;
    }

    void optionsBecomeDefaults()
    {
        SessionInfo info = sample(u"opts"_s);
        info.options.readOnly = true;
        info.options.queryTimeoutSec = 30;
        Session s(info, &m_driver, &m_secrets);
        const ExecOptions o = s.defaultExecOptions();
        QVERIFY(o.readOnly);
        QCOMPARE(o.fetchChunk, 500);
        QCOMPARE(o.timeout, std::chrono::milliseconds(30000));
        QVERIFY(s.isDatabaseVisible(u"app"_s));
        QVERIFY(!s.isDatabaseVisible(u"other"_s));
    }
};

QTEST_GUILESS_MAIN(TestSessions)
#include "tst_sessions.moc"
