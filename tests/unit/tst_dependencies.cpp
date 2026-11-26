// SPDX-License-Identifier: GPL-3.0-or-later
// Proves the third-party libraries link and work on every platform (M1.1, decision 124).
#include <QTest>
#include <qt6keychain/keychain.h>

#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/Theme>
#include <libpq-fe.h>
#include <sqlite3.h>
#include <utility>

using namespace Qt::StringLiterals;

class TestDependencies : public QObject
{
    Q_OBJECT

private slots:
    void libpqVersion()
    {
        // >= 17 for chunked-rows streaming (specs/driver-api.md §11).
        const int version = PQlibVersion();
        qInfo("libpq %d.%d", version / 10000, version % 10000);
        QVERIFY2(version >= 170000, qPrintable(QString::number(version)));
        QVERIFY(PQisthreadsafe() == 1); // one connection per worker thread (decision 042)
    }

    void sqliteVersionAndRoundTrip()
    {
        qInfo("SQLite %s", sqlite3_libversion());
        QVERIFY(sqlite3_libversion_number() >= 3035000); // RETURNING, DROP COLUMN (decision 048)
        QVERIFY(sqlite3_threadsafe() != 0);

        sqlite3 *db = nullptr;
        QCOMPARE(sqlite3_open(":memory:", &db), SQLITE_OK);
        const auto run = [db](const char *sql, int expectedStep) {
            sqlite3_stmt *stmt = nullptr;
            const bool prepared = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK;
            const bool stepped = prepared && sqlite3_step(stmt) == expectedStep;
            const int value
                = stepped && expectedStep == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : 0;
            sqlite3_finalize(stmt);
            if (!stepped)
                qWarning("SQLite: %s", sqlite3_errmsg(db));
            return std::pair{stepped, value};
        };
        QVERIFY(run("CREATE TABLE t(x INTEGER) STRICT", SQLITE_DONE).first);
        const auto [ok, value] = run("INSERT INTO t VALUES (42) RETURNING x", SQLITE_ROW);
        QVERIFY(ok);
        QCOMPARE(value, 42);
        sqlite3_close(db);
    }

    void keychainJobsCanBeCreated()
    {
        // No secret-store access in CI: just prove the library links and initialises.
        QKeychain::ReadPasswordJob job(u"OwlSql-test"_s);
        job.setKey(u"session/test/password"_s);
        QCOMPARE(job.service(), u"OwlSql-test"_s);
        QCOMPARE(job.key(), u"session/test/password"_s);
    }

    void syntaxDefinitionsAreBundled()
    {
        const KSyntaxHighlighting::Repository repository;
        for (const QString &name : {u"SQL (PostgreSQL)"_s, u"SQL (MySQL)"_s, u"SQL"_s, u"JSON"_s}) {
            QVERIFY2(repository.definitionForName(name).isValid(), qPrintable(name));
        }
        QVERIFY(repository.theme(u"Breeze Dark"_s).isValid());
        qInfo("KSyntaxHighlighting: %lld definitions, %lld themes",
              static_cast<long long>(repository.definitions().size()),
              static_cast<long long>(repository.themes().size()));
    }
};

QTEST_GUILESS_MAIN(TestDependencies)
#include "tst_dependencies.moc"
