// SPDX-License-Identifier: GPL-3.0-or-later
#include "appdatabase.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

namespace owl::app {

namespace {

bool exec(const QSqlDatabase &db, const QString &sql, QString *error)
{
    QSqlQuery query(db);
    if (query.exec(sql))
        return true;
    *error = QStringLiteral("%1\n  in: %2").arg(query.lastError().text(), sql.simplified());
    return false;
}

QStringList splitStatements(const QString &sql)
{
    // Our own migration files only: statements end with ';' at end of line; '--' line comments.
    QStringList statements;
    QString current;
    const QStringList lines = sql.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QLatin1String("--")))
            continue;
        current += line + QLatin1Char('\n');
        if (line.trimmed().endsWith(QLatin1Char(';'))) {
            if (!current.trimmed().isEmpty())
                statements << current.trimmed();
            current.clear();
        }
    }
    if (!current.trimmed().isEmpty())
        statements << current.trimmed();
    return statements;
}

} // namespace

AppDatabase::AppDatabase(QString connectionName)
    : m_connectionName(std::move(connectionName))
{ }

AppDatabase::~AppDatabase()
{
    close();
}

QList<AppDatabase::Migration> AppDatabase::builtinMigrations()
{
    static const QRegularExpression fileName(QStringLiteral("^(\\d{4})_[a-z0-9_]+\\.sql$"));
    QList<Migration> migrations;
    QDirIterator it(QStringLiteral(":/owlsql/migrations/owlsql"), {QStringLiteral("*.sql")},
                    QDir::Files);
    while (it.hasNext()) {
        const QFileInfo info = it.nextFileInfo();
        const QRegularExpressionMatch match = fileName.match(info.fileName());
        if (!match.hasMatch())
            continue;
        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        migrations.append(
            {.version = match.captured(1).toInt(), .sql = QString::fromUtf8(file.readAll())});
    }
    std::ranges::sort(migrations, {}, &Migration::version);
    return migrations;
}

bool AppDatabase::open(const QString &path, const QString &backupsDir, QString *error)
{
    return open(path, backupsDir, builtinMigrations(), error);
}

bool AppDatabase::open(const QString &path, const QString &backupsDir,
                       const QList<Migration> &migrations, QString *error)
{
    close();
    m_path = path;
    m_lastErrorKind = ErrorKind::Other; // until proven otherwise

    bool ok = false;
    {
        // Scoped: no QSqlDatabase handle may outlive this block if we close() on failure.
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(path);
        if (!db.open())
            *error = QStringLiteral("Cannot open %1: %2").arg(path, db.lastError().text());
        else
            ok = exec(db, QStringLiteral("PRAGMA journal_mode = WAL"), error)
                && exec(db, QStringLiteral("PRAGMA foreign_keys = ON"), error)
                && migrate(migrations, backupsDir, error);
    }
    if (!ok) {
        close();
        return false;
    }
    m_lastErrorKind = ErrorKind::None;
    return true;
}

void AppDatabase::close()
{
    if (!QSqlDatabase::contains(m_connectionName))
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool AppDatabase::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName) && database().isOpen();
}

QSqlDatabase AppDatabase::database() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

int AppDatabase::schemaVersion() const
{
    QSqlQuery query(database());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return -1;
    return query.value(0).toInt();
}

bool AppDatabase::migrate(const QList<Migration> &migrations, const QString &backupsDir,
                          QString *error)
{
    const int current = schemaVersion();
    const int latest = migrations.isEmpty() ? 0 : migrations.constLast().version;
    if (current < 0) {
        *error = QStringLiteral("Cannot read the schema version of %1").arg(m_path);
        return false;
    }
    if (current > latest) {
        m_lastErrorKind = ErrorKind::NewerSchema;
        *error = QStringLiteral("%1 was created by a newer OwlSql (schema v%2, this version "
                                "supports v%3). It was not modified.")
                     .arg(m_path)
                     .arg(current)
                     .arg(latest);
        return false;
    }
    if (current == latest)
        return true;

    if (current > 0 && !backup(current, backupsDir, error))
        return false;

    QSqlDatabase db = database();
    for (const Migration &migration : migrations) {
        if (migration.version <= current)
            continue;
        if (!db.transaction()) {
            *error = db.lastError().text();
            return false;
        }
        const QStringList statements = splitStatements(migration.sql);
        for (const QString &statement : statements) {
            if (!exec(db, statement, error)) {
                db.rollback();
                *error
                    = QStringLiteral("Migration %1 failed: %2").arg(migration.version).arg(*error);
                return false;
            }
        }
        if (!exec(db, QStringLiteral("PRAGMA user_version = %1").arg(migration.version), error)
            || !db.commit()) {
            db.rollback();
            return false;
        }
    }
    return true;
}

bool AppDatabase::backup(int fromVersion, const QString &backupsDir, QString *error)
{
    if (!QDir().mkpath(backupsDir)) {
        *error = QStringLiteral("Cannot create backup directory %1").arg(backupsDir);
        return false;
    }
    const QString base = QFileInfo(m_path).completeBaseName();
    const QString target = QDir(backupsDir)
                               .filePath(QStringLiteral("%1-v%2-%3.db")
                                             .arg(base)
                                             .arg(fromVersion)
                                             .arg(QDateTime::currentDateTimeUtc().toString(
                                                 QStringLiteral("yyyyMMdd-HHmmss-zzz"))));

    QSqlQuery query(database());
    query.prepare(QStringLiteral("VACUUM INTO ?"));
    query.addBindValue(target);
    if (!query.exec()) {
        *error = QStringLiteral("Backup before migration failed: %1").arg(query.lastError().text());
        return false;
    }

    // Keep only the newest backups of this database.
    QDir dir(backupsDir);
    const QFileInfoList backups = dir.entryInfoList({base + QStringLiteral("-v*.db")}, QDir::Files,
                                                    QDir::Time); // newest first
    for (qsizetype i = keptBackups; i < backups.size(); ++i)
        QFile::remove(backups.at(i).filePath());
    return true;
}

} // namespace owl::app
