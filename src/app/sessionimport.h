// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/result.h"
#include "sessioninfo.h"

#include <QByteArray>
#include <QJsonDocument>

namespace owl {
class DriverRegistry;
}

namespace owl::app {

class SecretStore;
class SessionStore;

// Sessions coming from outside: a team export, a connection URL, ~/.pgpass, ~/.my.cnf
// (app-store.md §6, decision 085). Every source produces an ImportBundle; applyImport() merges it.
struct ImportedSession
{
    SessionInfo info;
    QString password; // from the source (URL, .pgpass, .my.cnf); never from JSON exports
};

struct ImportBundle
{
    QList<FolderInfo> folders;
    QList<ImportedSession> sessions;
    QStringList warnings; // skipped entries, missing certificate files…
};

// *.owlsql-sessions.json, format "owlsql-sessions" version 1. Secrets are never written.
QJsonDocument exportSessions(const QList<FolderInfo> &folders, const QList<SessionInfo> &sessions);
// Keychain secret modes become "ask": the secrets stayed on the other machine.
Result<ImportBundle> importSessionsJson(const QByteArray &json);

// One session from a URL any registered driver understands ("postgresql://…", "mysql://…", a
// .db path). Named after its user, host and database.
Result<ImportBundle> importFromUrl(const DriverRegistry &drivers, const QString &url);

// libpq's password file: hostname:port:database:username:password, "\" escapes ":" and "\".
// Lines with a wildcard host or user are skipped (they are not a connection).
ImportBundle importPgpass(const QString &text);

// MySQL option file: [client] / [mysql] give one session, every [client<suffix>] group another
// (with [client]'s values as defaults). !include directives are reported, not followed.
ImportBundle importMyCnf(const QString &text);

enum class ConflictPolicy : quint8 { KeepBoth, Replace, Skip };

// Writes the bundle's folders and sessions (merging by id) and stores provided passwords in
// `secrets` (password mode "keychain"; "ask" when there is no keychain). Returns the sessions
// written.
Result<QList<SessionInfo>> applyImport(SessionStore &store, SecretStore *secrets,
                                       const ImportBundle &bundle,
                                       ConflictPolicy policy = ConflictPolicy::KeepBoth);

} // namespace owl::app
