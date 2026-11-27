// SPDX-License-Identifier: GPL-3.0-or-later
// Runtime sessions against real servers: the startup script runs on every physical connection,
// PostgreSQL gets one connection (and catalog) per browsed database, MySQL browses every database
// through the meta connection (decision 043).
#include "mysqldriver.h"
#include "pgdriver.h"
#include "secretstore.h"
#include "session.h"
#include "testserver.h"

#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;
using namespace owl::app;

namespace {

// The first value of a statement run through a worker (as the app runs queries).
Value firstValue(ConnectionWorker *worker, const QString &sql)
{
    Statement s;
    s.sql = sql;
    QueryJob *job = worker->execute(s);
    Value out;
    QObject::connect(job, &QueryJob::rowsReady, job, [&out](const RowBatch &b) {
        if (b.rowCount() > 0 && out.isNull())
            out = b.at(0, 0);
    });
    if (!QTest::qWaitFor([job] { return job->isDone(); }, 10000))
        qWarning("statement did not finish: %s", qPrintable(sql));
    if (job->state() == QueryJob::State::Failed)
        qWarning("%s: %s", qPrintable(sql), qPrintable(job->error().toString()));
    return out;
}

SessionInfo sessionFor(const test::TestServer &server, const QString &script)
{
    SessionInfo info;
    info.id = newId();
    info.name = u"test"_s;
    info.driverId = server.params.driverId;
    info.params = server.params;
    info.params.password.clear();
    info.options.startupScript = script;
    info.secretModes.insert(secret::Password, SecretMode::Keychain);
    return info;
}

} // namespace

class TestSessionServers : public QObject
{
    Q_OBJECT

    test::TestServer m_pg = test::testServer("OWL_TEST_PG_URL", u"postgres"_s);
    test::TestServer m_mysql = test::testServer("OWL_TEST_MYSQL_URL", u"mysql"_s);
    postgres::PgDriver m_pgDriver;
    mysql::MySqlDriver m_mysqlDriver;
    MemorySecretStore m_secrets;

    static void need(const test::TestServer &server)
    {
        if (!server.error.isEmpty())
            QFAIL(qPrintable(server.error));
        if (!server.skipReason.isEmpty())
            QSKIP(qPrintable(server.skipReason));
    }

    void exec(IConnection &conn, const QString &sql)
    {
        Statement s;
        s.sql = sql;
        auto c = conn.execute(s, {});
        QVERIFY2(c, qPrintable(sql + u": "_s + (c ? QString() : c.error().toString())));
        (*c)->close();
    }

private slots:
    void postgresDatabasesGetTheirOwnConnection()
    {
        need(m_pg);
        {
            std::unique_ptr<IConnection> admin = m_pgDriver.createConnection();
            QVERIFY(admin->open(m_pg.params));
            exec(*admin, u"DROP DATABASE IF EXISTS owl_session_db"_s);
            exec(*admin, u"CREATE DATABASE owl_session_db"_s);
        }
        const SessionInfo info = sessionFor(m_pg,
                                            u"SET search_path TO pg_catalog, public;"
                                            u"SET application_name = 'owl-startup'"_s);
        m_secrets.write(info.id, secret::Password, m_pg.params.password);
        Session session(info, &m_pgDriver, &m_secrets);
        QSignalSpy connected(&session, &Session::connected);
        session.connectToServer();
        QVERIFY(connected.wait(15000));
        QVERIFY(session.usesDatabaseConnections());
        QCOMPARE(firstValue(session.metaWorker(), u"SHOW application_name"_s).toText(),
                 u"owl-startup"_s);

        // The meta catalog cannot browse another database; the session routes it.
        CatalogCache *other = session.catalogFor(u"owl_session_db"_s);
        QVERIFY(other);
        QVERIFY(other != session.catalog());
        QCOMPARE(session.catalogFor(m_pg.params.database), session.catalog());
        const ObjectPath db
            = ObjectPath{}.child({.kind = kind::Database, .name = u"owl_session_db"_s, .oid = {}});
        QSignalSpy loaded(other, &CatalogCache::loaded);
        other->loadChildren(db);
        QVERIFY(loaded.wait(15000));
        bool hasPublic = false;
        for (const ObjectNode &n : other->children(db).value_or(QList<ObjectNode>{}))
            hasPublic = hasPublic || n.ref.name == u"public";
        QVERIFY(hasPublic);

        ConnectionWorker *worker = session.workerFor(u"owl_session_db"_s);
        QCOMPARE(firstValue(worker, u"SELECT current_database()"_s).toText(), u"owl_session_db"_s);
        QCOMPARE(firstValue(worker, u"SHOW application_name"_s).toText(), u"owl-startup"_s);

        session.disconnectFromServer();
        QTest::qWait(200); // let the servers see the connections close
        std::unique_ptr<IConnection> admin = m_pgDriver.createConnection();
        QVERIFY(admin->open(m_pg.params));
        exec(*admin, u"DROP DATABASE IF EXISTS owl_session_db"_s);
    }

    void postgresWrongPasswordFails()
    {
        need(m_pg);
        const SessionInfo info = sessionFor(m_pg, {});
        m_secrets.write(info.id, secret::Password, u"definitely-wrong"_s);
        Session session(info, &m_pgDriver, &m_secrets);
        QSignalSpy failed(&session, &Session::failed);
        session.connectToServer();
        QVERIFY(failed.wait(15000));
        QCOMPARE(session.lastError().category, DbError::Category::Auth);
    }

    void mysqlBrowsesEveryDatabaseThroughMeta()
    {
        need(m_mysql);
        const SessionInfo info = sessionFor(m_mysql, u"SET @owl_startup = 42"_s);
        m_secrets.write(info.id, secret::Password, m_mysql.params.password);
        Session session(info, &m_mysqlDriver, &m_secrets);
        QSignalSpy connected(&session, &Session::connected);
        session.connectToServer();
        QVERIFY(connected.wait(15000));
        QVERIFY(!session.usesDatabaseConnections());
        QCOMPARE(session.catalogFor(u"mysql"_s), session.catalog());
        QCOMPARE(firstValue(session.metaWorker(), u"SELECT @owl_startup"_s).toInt(), 42);
    }
};

QTEST_GUILESS_MAIN(TestSessionServers)
#include "tst_session_servers.moc"
