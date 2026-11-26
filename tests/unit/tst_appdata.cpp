// SPDX-License-Identifier: GPL-3.0-or-later
// owlsql.db migrations and the UiState store.
#include "appdatabase.h"
#include "uistate.h"

#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using owl::app::AppDatabase;
using owl::app::UiState;

class TestAppData : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString dbPath() const { return m_dir.filePath(u"owlsql.db"_s); }
    QString backups() const { return m_dir.filePath(u"backups"_s); }

    static int rowCount(const AppDatabase &db, const QString &table)
    {
        QSqlQuery query(db.database());
        return query.exec(u"SELECT count(*) FROM "_s + table) && query.next()
            ? query.value(0).toInt()
            : -1;
    }

private slots:
    void init()
    {
        QDir(m_dir.path()).removeRecursively();
        QDir().mkpath(m_dir.path());
    }

    void builtinMigrationsAreNumbered()
    {
        const QList<AppDatabase::Migration> migrations = AppDatabase::builtinMigrations();
        QVERIFY(!migrations.isEmpty());
        for (qsizetype i = 0; i < migrations.size(); ++i)
            QCOMPARE(migrations.at(i).version, i + 1); // 1, 2, 3... without gaps
    }

    void createsFreshDatabase()
    {
        AppDatabase db(u"t-fresh"_s);
        QString error;
        QVERIFY2(db.open(dbPath(), backups(), &error), qPrintable(error));
        QCOMPARE(db.schemaVersion(), AppDatabase::builtinMigrations().constLast().version);
        QCOMPARE(rowCount(db, u"ui_state"_s), 0);
        QVERIFY(QDir(backups()).isEmpty()); // nothing to back up on first creation
    }

    void migratesWithBackupAndKeepsData()
    {
        const QList<AppDatabase::Migration> v1
            = {{.version = 1, .sql = u"CREATE TABLE a (x INTEGER);"_s}};
        QList<AppDatabase::Migration> v2 = v1;
        v2.append(
            {.version = 2, .sql = u"CREATE TABLE b (y TEXT);\nINSERT INTO b VALUES ('z');"_s});

        QString error;
        {
            AppDatabase db(u"t-v1"_s);
            QVERIFY2(db.open(dbPath(), backups(), v1, &error), qPrintable(error));
            QVERIFY(QSqlQuery(db.database()).exec(u"INSERT INTO a VALUES (42)"_s));
        }
        AppDatabase db(u"t-v2"_s);
        QVERIFY2(db.open(dbPath(), backups(), v2, &error), qPrintable(error));
        QCOMPARE(db.schemaVersion(), 2);
        QCOMPARE(rowCount(db, u"a"_s), 1);
        QCOMPARE(rowCount(db, u"b"_s), 1);
        const QStringList files = QDir(backups()).entryList({u"owlsql-v1-*.db"_s}, QDir::Files);
        QCOMPARE(files.size(), 1);
    }

    void failedMigrationRollsBack()
    {
        const QList<AppDatabase::Migration> v1
            = {{.version = 1, .sql = u"CREATE TABLE a (x INTEGER);"_s}};
        QList<AppDatabase::Migration> broken = v1;
        broken.append({.version = 2, .sql = u"CREATE TABLE b (y TEXT);\nTHIS IS NOT SQL;"_s});

        QString error;
        {
            AppDatabase db(u"t-ok"_s);
            QVERIFY2(db.open(dbPath(), backups(), v1, &error), qPrintable(error));
        }
        {
            AppDatabase db(u"t-broken"_s);
            QVERIFY(!db.open(dbPath(), backups(), broken, &error));
            QVERIFY2(error.contains(u"Migration 2"_s), qPrintable(error));
            QCOMPARE(db.lastErrorKind(), AppDatabase::ErrorKind::Other);
        }
        AppDatabase db(u"t-check"_s);
        QVERIFY(db.open(dbPath(), backups(), v1, &error));
        QCOMPARE(db.schemaVersion(), 1);
        QSqlQuery query(db.database());
        QVERIFY(!query.exec(u"SELECT * FROM b"_s)); // table b was rolled back
    }

    void refusesNewerDatabase()
    {
        QString error;
        {
            AppDatabase db(u"t-new"_s);
            QVERIFY(db.open(dbPath(), backups(), &error));
            QVERIFY(QSqlQuery(db.database()).exec(u"PRAGMA user_version = 999"_s));
        }
        AppDatabase db(u"t-old-app"_s);
        QVERIFY(!db.open(dbPath(), backups(), &error));
        QCOMPARE(db.lastErrorKind(), AppDatabase::ErrorKind::NewerSchema);
        QVERIFY2(error.contains(u"newer OwlSql"_s), qPrintable(error));
        QVERIFY(!db.isOpen());
    }

    void keepsOnlyNewestBackups()
    {
        QList<AppDatabase::Migration> migrations;
        QString error;
        for (int version = 1; version <= AppDatabase::keptBackups + 3; ++version) {
            migrations.append(
                {.version = version, .sql = u"CREATE TABLE t%1 (x INTEGER);"_s.arg(version)});
            AppDatabase db(u"t-backup-%1"_s.arg(version));
            QVERIFY2(db.open(dbPath(), backups(), migrations, &error), qPrintable(error));
        }
        QCOMPARE(QDir(backups()).entryList({u"owlsql-v*.db"_s}, QDir::Files).size(),
                 AppDatabase::keptBackups);
    }

    void uiStatePersists()
    {
        QString error;
        {
            AppDatabase db(u"t-ui1"_s);
            QVERIFY2(db.open(dbPath(), backups(), &error), qPrintable(error));
            UiState state(&db, nullptr);
            state.setValue(u"layout.sidebarWidth"_s, 312);
            state.setValue(u"layout.sidebarVisible"_s, false);
            state.setValue(u"workspace.tabs"_s, QVariantList{u"welcome"_s});
            QCOMPARE(state.value(u"layout.sidebarWidth"_s).toInt(), 312);
        } // destructor flushes
        AppDatabase db(u"t-ui2"_s);
        QVERIFY(db.open(dbPath(), backups(), &error));
        UiState state(&db, nullptr);
        QCOMPARE(state.value(u"layout.sidebarWidth"_s).toInt(), 312);
        QCOMPARE(state.value(u"layout.sidebarVisible"_s, true).toBool(), false);
        QCOMPARE(state.value(u"workspace.tabs"_s).toList(), QVariantList{u"welcome"_s});
        QCOMPARE(state.value(u"missing"_s, 7).toInt(), 7);
    }

    void uiStateWorksWithoutDatabase()
    {
        UiState state(nullptr, nullptr);
        state.setValue(u"k"_s, u"v"_s);
        QCOMPARE(state.value(u"k"_s).toString(), u"v"_s);
        state.flush(); // no crash, nothing to write to
    }
};

QTEST_GUILESS_MAIN(TestAppData)
#include "tst_appdata.moc"
