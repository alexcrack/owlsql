// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionlistmodel.h"

#include "sessionmanager.h"

#include <QFileInfo>

namespace owl::app {

namespace {

// "app@db.internal:5432/app_db", "/data/app.db".
QString target(const SessionInfo &s)
{
    const ConnectionParams &p = s.params;
    if (s.driverId == u"sqlite")
        return p.database;
    QString out = p.user.isEmpty() ? QString() : p.user + u'@';
    out += p.transport == ConnectionParams::Transport::Socket ? QFileInfo(p.socketPath).fileName()
                                                              : p.host;
    if (p.port != 0)
        out += u':' + QString::number(p.port);
    if (!p.database.isEmpty())
        out += u'/' + p.database;
    return out;
}

} // namespace

QString defaultTag(SafetyLevel level)
{
    switch (level) {
    case SafetyLevel::Staging:
        return QStringLiteral("blue");
    case SafetyLevel::Prod:
        return QStringLiteral("red");
    case SafetyLevel::Dev:
        break;
    }
    return QStringLiteral("green");
}

SessionListModel::SessionListModel(SessionManager *manager, QObject *parent)
    : QAbstractListModel(parent)
    , m_manager(manager)
{ }

int SessionListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QHash<int, QByteArray> SessionListModel::roleNames() const
{
    return {{KindRole, "kind"},         {ItemIdRole, "itemId"},
            {NameRole, "name"},         {SubtitleRole, "subtitle"},
            {DepthRole, "depth"},       {ExpandedRole, "expanded"},
            {DriverIdRole, "driverId"}, {LevelRole, "level"},
            {TagRole, "tag"},           {StateRole, "connectionState"},
            {LastUsedRole, "lastUsed"}, {ChildCountRole, "childCount"}};
}

QVariant SessionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows.at(index.row());
    if (role == KindRole)
        return row.folder ? QStringLiteral("folder") : QStringLiteral("session");
    if (role == ItemIdRole)
        return row.id;
    if (role == DepthRole)
        return row.depth;

    if (row.folder) {
        const FolderInfo &f = m_folders.at(m_folderIndex.value(row.id));
        switch (role) {
        case NameRole:
            return f.name;
        case ExpandedRole:
            return f.expanded;
        case ChildCountRole:
            return childCount(f.id);
        default:
            return {};
        }
    }

    const SessionInfo &s = m_sessions.at(m_sessionIndex.value(row.id));
    switch (role) {
    case NameRole:
        return s.name;
    case SubtitleRole: {
        const QVariantMap info = m_manager->info(s.id);
        QString subtitle = info.value(QStringLiteral("driverName")).toString();
        if (const QString t = target(s); !t.isEmpty())
            subtitle += QStringLiteral(" · ") + t;
        if (!m_filter.isEmpty() && !s.folderId.isEmpty())
            subtitle = folderPath(s.folderId) + QStringLiteral(" · ") + subtitle;
        return subtitle;
    }
    case DriverIdRole:
        return s.driverId;
    case LevelRole:
        return safetyLevelName(s.safetyLevel);
    case TagRole:
        return s.color.isEmpty() ? defaultTag(s.safetyLevel) : s.color;
    case StateRole:
        return m_manager->state(s.id);
    case LastUsedRole:
        return s.lastUsedAt;
    default:
        return {};
    }
}

void SessionListModel::setFilter(const QString &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    rebuild();
    emit filterChanged();
}

int SessionListModel::rowOf(const QString &itemId) const
{
    for (qsizetype i = 0; i < m_rows.size(); ++i) {
        if (m_rows.at(i).id == itemId)
            return static_cast<int>(i);
    }
    return -1;
}

void SessionListModel::setContent(QList<FolderInfo> folders, QList<SessionInfo> sessions)
{
    m_folders = std::move(folders);
    m_sessions = std::move(sessions);
    m_folderIndex.clear();
    m_sessionIndex.clear();
    for (qsizetype i = 0; i < m_folders.size(); ++i)
        m_folderIndex.insert(m_folders.at(i).id, i);
    for (qsizetype i = 0; i < m_sessions.size(); ++i)
        m_sessionIndex.insert(m_sessions.at(i).id, i);
    rebuild();
}

void SessionListModel::refreshStates()
{
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1),
                         {StateRole, SubtitleRole});
}

void SessionListModel::addFolder(const FolderInfo &folder, int depth)
{
    m_rows.append({.folder = true, .id = folder.id, .depth = depth});
    if (!folder.expanded)
        return;
    for (const FolderInfo &f : std::as_const(m_folders)) {
        if (f.parentId == folder.id)
            addFolder(f, depth + 1);
    }
    for (const SessionInfo &s : std::as_const(m_sessions)) {
        if (s.folderId == folder.id)
            m_rows.append({.folder = false, .id = s.id, .depth = depth + 1});
    }
}

void SessionListModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    if (!m_filter.isEmpty()) {
        for (const SessionInfo &s : std::as_const(m_sessions)) {
            const bool match = s.name.contains(m_filter, Qt::CaseInsensitive)
                || target(s).contains(m_filter, Qt::CaseInsensitive)
                || s.notes.contains(m_filter, Qt::CaseInsensitive);
            if (match)
                m_rows.append({.folder = false, .id = s.id, .depth = 0});
        }
    } else {
        for (const FolderInfo &f : std::as_const(m_folders)) {
            if (f.parentId.isEmpty())
                addFolder(f, 0);
        }
        for (const SessionInfo &s : std::as_const(m_sessions)) {
            if (s.folderId.isEmpty())
                m_rows.append({.folder = false, .id = s.id, .depth = 0});
        }
    }
    endResetModel();
    emit countsChanged();
}

QString SessionListModel::folderPath(const QString &folderId) const
{
    QStringList parts;
    QString id = folderId;
    for (int guard = 0; !id.isEmpty() && guard < 32; ++guard) {
        const auto it = m_folderIndex.constFind(id);
        if (it == m_folderIndex.cend())
            break;
        parts.prepend(m_folders.at(*it).name);
        id = m_folders.at(*it).parentId;
    }
    return parts.join(QStringLiteral(" / "));
}

int SessionListModel::childCount(const QString &folderId) const
{
    int n = 0;
    for (const SessionInfo &s : m_sessions) {
        if (s.folderId == folderId)
            ++n;
    }
    for (const FolderInfo &f : m_folders) {
        if (f.parentId == folderId)
            n += childCount(f.id);
    }
    return n;
}

} // namespace owl::app
