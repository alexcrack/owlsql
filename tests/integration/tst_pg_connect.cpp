// SPDX-License-Identifier: GPL-3.0-or-later
// First server integration test (M1.1, decision 125): proves CI's PostgreSQL service is reachable
// through libpq. Skipped unless OWL_TEST_PG_URL is set, e.g.
//   OWL_TEST_PG_URL=postgresql://postgres:owlsql@localhost:5432/postgres
#include <QTest>

#include <libpq-fe.h>
#include <memory>

class TestPgConnect : public QObject
{
    Q_OBJECT

private slots:
    void connectsAndQueriesServerVersion()
    {
        const QByteArray url = qgetenv("OWL_TEST_PG_URL");
        if (url.isEmpty()) {
            // CI sets OWL_REQUIRE_INTEGRATION_TESTS where a server is provided, so a broken setup
            // fails instead of silently skipping (ctest reports skipped tests as passed).
            if (qEnvironmentVariableIsSet("OWL_REQUIRE_INTEGRATION_TESTS"))
                QFAIL("OWL_TEST_PG_URL not set but OWL_REQUIRE_INTEGRATION_TESTS is");
            QSKIP("OWL_TEST_PG_URL not set — no PostgreSQL server to test against");
        }

        const std::unique_ptr<PGconn, decltype(&PQfinish)> conn(PQconnectdb(url.constData()),
                                                                &PQfinish);
        QVERIFY(conn);
        QVERIFY2(PQstatus(conn.get()) == CONNECTION_OK, PQerrorMessage(conn.get()));

        const int serverVersion = PQserverVersion(conn.get());
        qInfo("PostgreSQL server %d.%d", serverVersion / 10000, serverVersion % 10000);
        QVERIFY(serverVersion >= 130000); // minimum supported server (decision 048)

        const std::unique_ptr<PGresult, decltype(&PQclear)> result(
            PQexec(conn.get(), "SELECT 6 * 7"), &PQclear);
        QCOMPARE(PQresultStatus(result.get()), PGRES_TUPLES_OK);
        QCOMPARE(QByteArray(PQgetvalue(result.get(), 0, 0)), QByteArray("42"));
    }
};

QTEST_GUILESS_MAIN(TestPgConnect)
#include "tst_pg_connect.moc"
