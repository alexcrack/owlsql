// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/result.h"
#include "sessioninfo.h"

#include <QObject>
#include <QSqlDatabase>

namespace owl::app {

// Saved sessions and their folders in owlsql.db (app-store.md §3). Main thread only. Never stores
// secrets: a password in SessionInfo::params is dropped on save.
class SessionStore : public QObject
{
    Q_OBJECT

public:
    explicit SessionStore(const QSqlDatabase &database, QObject *parent = nullptr);

    QList<FolderInfo> folders() const; // by parent, then position
    QList<SessionInfo> sessions() const; // by folder, position, then name
    std::optional<SessionInfo> session(const QString &id) const;
    std::optional<FolderInfo> folder(const QString &id) const;

    Result<FolderInfo> addFolder(const QString &name, const QString &parentId = {});
    Result<void> updateFolder(const FolderInfo &folder);
    // Subfolders are removed too; their sessions move to the top level.
    Result<void> removeFolder(const QString &id);

    // Inserts (a new id when empty; position at the end of its folder) or updates. Sets
    // created/updated times; returns the session as stored.
    Result<SessionInfo> saveSession(SessionInfo session);
    Result<void> removeSession(const QString &id);
    // "name (copy)" in the same folder; secrets are not copied (keychain modes become "ask").
    Result<SessionInfo> duplicateSession(const QString &id);
    Result<void> moveSession(const QString &id, const QString &folderId, int position);
    Result<void> markUsed(const QString &id);

signals:
    void changed();

private:
    QSqlDatabase m_db;
};

} // namespace owl::app
