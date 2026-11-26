// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QSqlDatabase>
#include <QString>

namespace owl::app {

// OwlSql's own SQLite store (owlsql.db). Forward-only numbered migrations embedded as resources,
// version in PRAGMA user_version, automatic backup before migrating existing data, and refusal
// to touch a database created by a newer OwlSql (docs/specs/app-store.md §8).
// Uses QtSql's bundled SQLite: this is local app storage, not a user database driver.
class AppDatabase
{
public:
    enum class ErrorKind : quint8 { None, NewerSchema, Other };

    struct Migration
    {
        int version = 0;
        QString sql; // one statement per ';' at end of line
    };

    explicit AppDatabase(QString connectionName = QStringLiteral("owlsql-app"));
    ~AppDatabase();
    AppDatabase(const AppDatabase &) = delete;
    AppDatabase &operator=(const AppDatabase &) = delete;

    // Opens (creating if needed) and migrates to the latest version. On failure returns false
    // and sets *error; the database is left unchanged.
    bool open(const QString &path, const QString &backupsDir, QString *error);
    bool open(const QString &path, const QString &backupsDir, const QList<Migration> &migrations,
              QString *error);
    void close();

    // Why the last open() failed: NewerSchema means "created by a newer OwlSql, left untouched".
    ErrorKind lastErrorKind() const { return m_lastErrorKind; }

    bool isOpen() const;
    int schemaVersion() const;
    QSqlDatabase database() const;

    // Migrations bundled under :/owlsql/migrations/owlsql/NNNN_name.sql, sorted by version.
    static QList<Migration> builtinMigrations();
    static constexpr int keptBackups = 3;

private:
    bool migrate(const QList<Migration> &migrations, const QString &backupsDir, QString *error);
    bool backup(int fromVersion, const QString &backupsDir, QString *error);

    QString m_connectionName;
    QString m_path;
    ErrorKind m_lastErrorKind = ErrorKind::None;
};

} // namespace owl::app
