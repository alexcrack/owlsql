// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionstore.h"

#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>

namespace owl::app {

namespace {

DbError sqlError(const QSqlQuery &q)
{
    DbError e;
    e.message = q.lastError().text();
    return e;
}

QString json(const QJsonObject &o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QJsonObject object(const QVariant &v)
{
    return QJsonDocument::fromJson(v.toString().toUtf8()).object();
}

QVariant nullable(const QString &s)
{
    return s.isEmpty() ? QVariant(QMetaType::fromType<QString>()) : QVariant(s);
}

QString timestamp(const QDateTime &t)
{
    return t.toUTC().toString(Qt::ISODateWithMs);
}

QDateTime parseTime(const QVariant &v)
{
    return v.isNull() ? QDateTime() : QDateTime::fromString(v.toString(), Qt::ISODateWithMs);
}

constexpr auto sessionColumns
    = "id, folder_id, position, name, driver_id, params, safety_level, color, options, notes, "
      "secret_modes, created_at, updated_at, last_used_at, imported_from";

SessionInfo readSession(const QSqlQuery &q)
{
    SessionInfo s;
    s.id = q.value(0).toString();
    s.folderId = q.value(1).toString();
    s.position = q.value(2).toInt();
    s.name = q.value(3).toString();
    s.driverId = q.value(4).toString();
    s.params = connectionParamsFromJson(object(q.value(5)), s.driverId);
    s.safetyLevel = safetyLevelFromName(q.value(6).toString());
    s.color = q.value(7).toString();
    s.options = SessionOptions::fromJson(object(q.value(8)));
    s.notes = q.value(9).toString();
    s.secretModes = secretModesFromJson(object(q.value(10)));
    s.createdAt = parseTime(q.value(11));
    s.updatedAt = parseTime(q.value(12));
    s.lastUsedAt = parseTime(q.value(13));
    s.importedFrom = q.value(14).toString();
    return s;
}

FolderInfo readFolder(const QSqlQuery &q)
{
    return {.id = q.value(0).toString(),
            .parentId = q.value(1).toString(),
            .name = q.value(2).toString(),
            .position = q.value(3).toInt(),
            .expanded = q.value(4).toBool()};
}

} // namespace

SessionStore::SessionStore(const QSqlDatabase &database, QObject *parent)
    : QObject(parent)
    , m_db(database)
{ }

QList<FolderInfo> SessionStore::folders() const
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT id, parent_id, name, position, expanded FROM folder "
                          "ORDER BY parent_id IS NOT NULL, parent_id, position, name"));
    QList<FolderInfo> out;
    while (q.next())
        out.append(readFolder(q));
    return out;
}

std::optional<FolderInfo> SessionStore::folder(const QString &id) const
{
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT id, parent_id, name, position, expanded FROM folder WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return readFolder(q);
}

QList<SessionInfo> SessionStore::sessions() const
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT %1 FROM session ORDER BY folder_id IS NOT NULL, folder_id, "
                          "position, name COLLATE NOCASE")
               .arg(QLatin1String(sessionColumns)));
    QList<SessionInfo> out;
    while (q.next())
        out.append(readSession(q));
    return out;
}

std::optional<SessionInfo> SessionStore::session(const QString &id) const
{
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("SELECT %1 FROM session WHERE id = ?").arg(QLatin1String(sessionColumns)));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return std::nullopt;
    return readSession(q);
}

Result<FolderInfo> SessionStore::addFolder(const QString &name, const QString &parentId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO folder (id, parent_id, name, position) VALUES (?, ?, ?, "
                             "(SELECT coalesce(max(position) + 1, 0) FROM folder "
                             "WHERE parent_id IS ?))"));
    const QString id = newId();
    q.addBindValue(id);
    q.addBindValue(nullable(parentId));
    q.addBindValue(name);
    q.addBindValue(nullable(parentId));
    if (!q.exec())
        return fail(sqlError(q));
    emit changed();
    if (std::optional<FolderInfo> added = folder(id))
        return *added;
    return fail(DbError::Category::Other, QStringLiteral("The folder was not stored"));
}

Result<void> SessionStore::updateFolder(const FolderInfo &f)
{
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral("UPDATE folder SET parent_id = ?, name = ?, position = ?, expanded = ? "
                       "WHERE id = ?"));
    q.addBindValue(nullable(f.parentId));
    q.addBindValue(f.name);
    q.addBindValue(f.position);
    q.addBindValue(f.expanded);
    q.addBindValue(f.id);
    if (!q.exec())
        return fail(sqlError(q));
    emit changed();
    return {};
}

Result<void> SessionStore::removeFolder(const QString &id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM folder WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec())
        return fail(sqlError(q));
    emit changed();
    return {};
}

Result<SessionInfo> SessionStore::saveSession(SessionInfo s)
{
    s.params.password.clear(); // secrets live in the keychain
    s.driverId = s.driverId.isEmpty() ? s.params.driverId : s.driverId;
    s.params.driverId = s.driverId;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    s.updatedAt = now;
    const bool exists = !s.id.isEmpty() && session(s.id).has_value();
    QSqlQuery q(m_db);
    if (!exists) {
        if (s.id.isEmpty())
            s.id = newId();
        s.createdAt = now;
        QSqlQuery pos(m_db);
        pos.prepare(QStringLiteral("SELECT coalesce(max(position) + 1, 0) FROM session "
                                   "WHERE folder_id IS ?"));
        pos.addBindValue(nullable(s.folderId));
        if (pos.exec() && pos.next())
            s.position = pos.value(0).toInt();
        q.prepare(
            QStringLiteral("INSERT INTO session (folder_id, position, name, driver_id, params, "
                           "safety_level, color, options, notes, secret_modes, created_at, "
                           "updated_at, last_used_at, imported_from, id) "
                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    } else {
        q.prepare(
            QStringLiteral("UPDATE session SET folder_id = ?, position = ?, name = ?, "
                           "driver_id = ?, params = ?, safety_level = ?, color = ?, "
                           "options = ?, notes = ?, secret_modes = ?, created_at = ?, "
                           "updated_at = ?, last_used_at = ?, imported_from = ? WHERE id = ?"));
        if (!s.createdAt.isValid()) {
            if (std::optional<SessionInfo> old = session(s.id))
                s.createdAt = old->createdAt;
        }
    }
    q.addBindValue(nullable(s.folderId));
    q.addBindValue(s.position);
    q.addBindValue(s.name);
    q.addBindValue(s.driverId);
    q.addBindValue(json(toJson(s.params)));
    q.addBindValue(safetyLevelName(s.safetyLevel));
    q.addBindValue(nullable(s.color));
    q.addBindValue(json(s.options.toJson()));
    q.addBindValue(nullable(s.notes));
    q.addBindValue(json(secretModesToJson(s.secretModes)));
    q.addBindValue(timestamp(s.createdAt));
    q.addBindValue(timestamp(s.updatedAt));
    q.addBindValue(s.lastUsedAt.isValid() ? QVariant(timestamp(s.lastUsedAt))
                                          : QVariant(QMetaType::fromType<QString>()));
    q.addBindValue(nullable(s.importedFrom));
    q.addBindValue(s.id);
    if (!q.exec())
        return fail(sqlError(q));
    emit changed();
    if (std::optional<SessionInfo> stored = session(s.id))
        return *stored;
    return fail(DbError::Category::Other, QStringLiteral("The session was not stored"));
}

Result<void> SessionStore::removeSession(const QString &id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM session WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec())
        return fail(sqlError(q));
    emit changed();
    return {};
}

Result<SessionInfo> SessionStore::duplicateSession(const QString &id)
{
    std::optional<SessionInfo> original = session(id);
    if (!original)
        return fail(DbError::Category::Other, QStringLiteral("No such session"));
    SessionInfo copy = *original;
    copy.id.clear();
    copy.name = QStringLiteral("%1 (copy)").arg(original->name);
    copy.lastUsedAt = {};
    for (auto it = copy.secretModes.begin(); it != copy.secretModes.end(); ++it) {
        if (it.value() == SecretMode::Keychain)
            it.value() = SecretMode::Ask;
    }
    return saveSession(copy);
}

Result<void> SessionStore::moveSession(const QString &id, const QString &folderId, int position)
{
    if (!m_db.transaction())
        return fail(DbError::Category::Other, m_db.lastError().text());
    QSqlQuery shift(m_db);
    shift.prepare(QStringLiteral("UPDATE session SET position = position + 1 "
                                 "WHERE folder_id IS ? AND position >= ? AND id <> ?"));
    shift.addBindValue(nullable(folderId));
    shift.addBindValue(position);
    shift.addBindValue(id);
    QSqlQuery move(m_db);
    move.prepare(QStringLiteral("UPDATE session SET folder_id = ?, position = ? WHERE id = ?"));
    move.addBindValue(nullable(folderId));
    move.addBindValue(position);
    move.addBindValue(id);
    if (!shift.exec() || !move.exec()) {
        const DbError e = sqlError(shift.lastError().isValid() ? shift : move);
        m_db.rollback();
        return fail(e);
    }
    m_db.commit();
    emit changed();
    return {};
}

Result<void> SessionStore::markUsed(const QString &id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE session SET last_used_at = ? WHERE id = ?"));
    q.addBindValue(timestamp(QDateTime::currentDateTimeUtc()));
    q.addBindValue(id);
    if (!q.exec())
        return fail(sqlError(q));
    emit changed();
    return {};
}

} // namespace owl::app
