// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "sessioninfo.h"

#include <QAbstractListModel>
#include <QQmlEngine>

namespace owl::app {

class SessionManager;

// Saved sessions and folders as a flat list for a ListView (design-system.md §8.2): folders
// (indented by depth, collapsible) with their sessions, then the sessions outside any folder.
// While a filter is set, only matching sessions are listed, each with its folder path.
class SessionListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by Sessions.model")
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged FINAL)
    Q_PROPERTY(int sessionCount READ sessionCount NOTIFY countsChanged FINAL)

public:
    enum Role : quint16 {
        KindRole = Qt::UserRole + 1, // "folder" | "session"
        ItemIdRole,
        NameRole,
        SubtitleRole, // sessions: driver · user@host/db; filtered: also the folder path
        DepthRole,
        ExpandedRole,
        DriverIdRole,
        LevelRole, // "dev" | "staging" | "prod"
        TagRole, // colour tag name (the level's default when not set)
        StateRole, // "" (closed) | "connecting" | "needsPassword" | "connected" | "failed"
        LastUsedRole, // QDateTime, invalid if never
        ChildCountRole, // folders: sessions inside (recursively)
    };

    explicit SessionListModel(SessionManager *manager, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    int sessionCount() const { return static_cast<int>(m_sessions.size()); }

    // The row showing `itemId`, -1 if hidden (inside a collapsed folder or filtered out).
    Q_INVOKABLE int rowOf(const QString &itemId) const;

    // New content from the store (the manager calls this whenever the store changes).
    void setContent(QList<FolderInfo> folders, QList<SessionInfo> sessions);
    void refreshStates(); // session states changed

signals:
    void filterChanged();
    void countsChanged();

private:
    struct Row
    {
        bool folder = false;
        QString id;
        int depth = 0;
    };

    void rebuild();
    void addFolder(const FolderInfo &folder, int depth);
    QString folderPath(const QString &folderId) const;
    int childCount(const QString &folderId) const;

    SessionManager *m_manager;
    QString m_filter;
    QList<FolderInfo> m_folders;
    QList<SessionInfo> m_sessions;
    QHash<QString, qsizetype> m_folderIndex;
    QHash<QString, qsizetype> m_sessionIndex;
    QList<Row> m_rows;
};

// The default tag colour of a safety level (design-system.md §3.4).
QString defaultTag(SafetyLevel level);

} // namespace owl::app
