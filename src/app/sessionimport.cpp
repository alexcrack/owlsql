// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessionimport.h"

#include "owl/core/driverregistry.h"
#include "secretstore.h"
#include "sessionstore.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>

namespace owl::app {

namespace {

constexpr QLatin1StringView formatName("owlsql-sessions");
constexpr int formatVersion = 1;

// "user@host:port/db", leaving out what is empty or default.
QString describe(const ConnectionParams &p)
{
    QString out = p.user.isEmpty() ? QString() : p.user + u'@';
    out += p.transport == ConnectionParams::Transport::Socket ? QFileInfo(p.socketPath).fileName()
                                                              : p.host;
    if (p.port != 0)
        out += u':' + QString::number(p.port);
    if (!p.database.isEmpty())
        out += u'/' + p.database;
    return out.isEmpty() ? p.driverId : out;
}

ImportedSession fromParams(ConnectionParams params, const QString &source)
{
    ImportedSession s;
    s.password = params.password;
    params.password.clear();
    s.info.driverId = params.driverId;
    s.info.name = describe(params);
    s.info.params = std::move(params);
    s.info.importedFrom = source;
    s.info.secretModes.insert(secret::Password,
                              s.password.isEmpty() ? SecretMode::Ask : SecretMode::Keychain);
    return s;
}

void warnMissingFiles(const SessionInfo &s, QStringList &warnings)
{
    for (const QString &file : {s.params.ssl.caFile, s.params.ssl.certFile, s.params.ssl.keyFile}) {
        if (!file.isEmpty() && !QFileInfo::exists(file))
            warnings << QStringLiteral("%1: file not found: %2").arg(s.name, file);
    }
}

// Splits a .pgpass line on unescaped ':' and removes the escapes.
QStringList pgpassFields(const QString &line)
{
    QStringList out{QString()};
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar c = line.at(i);
        if (c == u'\\' && i + 1 < line.size()) {
            out.last() += line.at(++i);
        } else if (c == u':') {
            out.append(QString());
        } else {
            out.last() += c;
        }
    }
    return out;
}

QString unquote(QString v)
{
    v = v.trimmed();
    if (v.size() >= 2 && (v.front() == u'"' || v.front() == u'\'') && v.back() == v.front())
        v = v.mid(1, v.size() - 2);
    return v;
}

} // namespace

QJsonDocument exportSessions(const QList<FolderInfo> &folders, const QList<SessionInfo> &sessions)
{
    QJsonArray f;
    for (const FolderInfo &folder : folders) {
        QJsonObject o{{QStringLiteral("id"), folder.id}, {QStringLiteral("name"), folder.name}};
        o.insert(QStringLiteral("parentId"),
                 folder.parentId.isEmpty() ? QJsonValue() : QJsonValue(folder.parentId));
        f.append(o);
    }
    QJsonArray s;
    for (const SessionInfo &session : sessions) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), session.id);
        o.insert(QStringLiteral("folderId"),
                 session.folderId.isEmpty() ? QJsonValue() : QJsonValue(session.folderId));
        o.insert(QStringLiteral("name"), session.name);
        o.insert(QStringLiteral("driverId"), session.driverId);
        o.insert(QStringLiteral("params"), toJson(session.params));
        o.insert(QStringLiteral("safetyLevel"), safetyLevelName(session.safetyLevel));
        if (!session.color.isEmpty())
            o.insert(QStringLiteral("color"), session.color);
        o.insert(QStringLiteral("options"), session.options.toJson());
        if (!session.notes.isEmpty())
            o.insert(QStringLiteral("notes"), session.notes);
        s.append(o);
    }
    return QJsonDocument(QJsonObject{
        {QStringLiteral("format"), QString(formatName)},
        {QStringLiteral("version"), formatVersion},
        {QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("folders"), f},
        {QStringLiteral("sessions"), s},
    });
}

Result<ImportBundle> importSessionsJson(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    const QJsonObject root = doc.object();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return fail(DbError::Category::Other,
                    QStringLiteral("Not a JSON document: %1").arg(parseError.errorString()));
    if (root.value(u"format").toString() != formatName)
        return fail(DbError::Category::Other, QStringLiteral("Not an OwlSql sessions file"));
    if (root.value(u"version").toInt() > formatVersion)
        return fail(DbError::Category::Other,
                    QStringLiteral("Created by a newer OwlSql (format version %1)")
                        .arg(root.value(u"version").toInt()));
    ImportBundle bundle;
    for (const auto &v : root.value(u"folders").toArray()) {
        const QJsonObject o = v.toObject();
        bundle.folders.append({.id = o.value(u"id").toString(),
                               .parentId = o.value(u"parentId").toString(),
                               .name = o.value(u"name").toString(),
                               .position = static_cast<int>(bundle.folders.size()),
                               .expanded = true});
    }
    for (const auto &v : root.value(u"sessions").toArray()) {
        const QJsonObject o = v.toObject();
        ImportedSession s;
        s.info.id = o.value(u"id").toString();
        s.info.folderId = o.value(u"folderId").toString();
        s.info.name = o.value(u"name").toString();
        s.info.driverId = o.value(u"driverId").toString();
        if (s.info.name.isEmpty() || s.info.driverId.isEmpty()) {
            bundle.warnings << QStringLiteral("Skipped a session without a name or driver");
            continue;
        }
        s.info.params = connectionParamsFromJson(o.value(u"params").toObject(), s.info.driverId);
        s.info.safetyLevel = safetyLevelFromName(o.value(u"safetyLevel").toString());
        s.info.color = o.value(u"color").toString();
        s.info.options = SessionOptions::fromJson(o.value(u"options").toObject());
        s.info.notes = o.value(u"notes").toString();
        s.info.importedFrom = QStringLiteral("json");
        s.info.secretModes.insert(secret::Password, SecretMode::Ask);
        warnMissingFiles(s.info, bundle.warnings);
        bundle.sessions.append(s);
    }
    return bundle;
}

Result<ImportBundle> importFromUrl(const DriverRegistry &drivers, const QString &url)
{
    Result<ConnectionParams> params = drivers.parseUrl(url);
    if (!params)
        return fail(params.error());
    ImportBundle bundle;
    bundle.sessions.append(fromParams(std::move(*params), QStringLiteral("url")));
    return bundle;
}

ImportBundle importPgpass(const QString &text)
{
    ImportBundle bundle;
    int lineNumber = 0;
    for (const QString &raw : text.split(u'\n')) {
        ++lineNumber;
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(u'#'))
            continue;
        const QStringList f = pgpassFields(line);
        if (f.size() != 5) {
            bundle.warnings << QStringLiteral(".pgpass line %1: expected 5 fields").arg(lineNumber);
            continue;
        }
        if (f.at(0) == u"*" || f.at(3) == u"*") {
            bundle.warnings
                << QStringLiteral(".pgpass line %1: wildcard host or user skipped").arg(lineNumber);
            continue;
        }
        ConnectionParams p;
        p.driverId = QStringLiteral("postgres");
        if (f.at(0).startsWith(u'/')) {
            p.transport = ConnectionParams::Transport::Socket;
            p.socketPath = f.at(0);
        } else {
            p.host = f.at(0);
        }
        if (f.at(1) != u"*")
            p.port = static_cast<quint16>(f.at(1).toUInt());
        p.database = f.at(2) == u"*" ? QString() : f.at(2);
        p.user = f.at(3);
        p.password = f.at(4);
        bundle.sessions.append(fromParams(std::move(p), QStringLiteral("pgpass")));
    }
    return bundle;
}

ImportBundle importMyCnf(const QString &text)
{
    ImportBundle bundle;
    QList<std::pair<QString, QVariantMap>> groups; // in file order
    QString current;
    for (const QString &raw : text.split(u'\n')) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(u'#') || line.startsWith(u';'))
            continue;
        if (line.startsWith(u'!')) {
            bundle.warnings << QStringLiteral("%1: not followed").arg(line);
            continue;
        }
        if (line.startsWith(u'[') && line.endsWith(u']')) {
            current = line.mid(1, line.size() - 2).trimmed().toLower();
            if (std::ranges::none_of(groups, [&](const auto &g) { return g.first == current; }))
                groups.append({current, {}});
            continue;
        }
        if (current.isEmpty())
            continue;
        const qsizetype eq = line.indexOf(u'=');
        const QString key = (eq < 0 ? line : line.left(eq)).trimmed().toLower().replace(u'_', u'-');
        const QString value = eq < 0 ? QString() : unquote(line.mid(eq + 1));
        for (auto &g : groups) {
            if (g.first == current)
                g.second.insert(key, value);
        }
    }
    const auto values = [&](const QString &name) {
        for (const auto &g : groups) {
            if (g.first == name)
                return g.second;
        }
        return QVariantMap();
    };
    const QVariantMap client = values(QStringLiteral("client"));
    const auto make = [&](const QString &groupName, QVariantMap v) {
        for (auto it = client.cbegin(); it != client.cend(); ++it) {
            if (!v.contains(it.key()))
                v.insert(it.key(), it.value());
        }
        if (!v.contains(QStringLiteral("host")) && !v.contains(QStringLiteral("socket"))
            && !v.contains(QStringLiteral("user")))
            return;
        ConnectionParams p;
        p.driverId = QStringLiteral("mysql");
        p.host = v.value(QStringLiteral("host")).toString();
        p.port = static_cast<quint16>(v.value(QStringLiteral("port")).toUInt());
        p.user = v.value(QStringLiteral("user")).toString();
        p.password = v.value(QStringLiteral("password")).toString();
        p.database = v.value(QStringLiteral("database")).toString();
        if (v.contains(QStringLiteral("socket")) && p.host.isEmpty()) {
            p.transport = ConnectionParams::Transport::Socket;
            p.socketPath = v.value(QStringLiteral("socket")).toString();
        }
        if (const auto mode = sslModeFromName(v.value(QStringLiteral("ssl-mode")).toString()))
            p.ssl.mode = *mode;
        p.ssl.caFile = v.value(QStringLiteral("ssl-ca")).toString();
        p.ssl.certFile = v.value(QStringLiteral("ssl-cert")).toString();
        p.ssl.keyFile = v.value(QStringLiteral("ssl-key")).toString();
        ImportedSession s = fromParams(std::move(p), QStringLiteral("mycnf"));
        s.info.name = QStringLiteral("%1 (%2)").arg(s.info.name, groupName);
        bundle.sessions.append(s);
    };
    QVariantMap mysql = values(QStringLiteral("mysql"));
    make(QStringLiteral("client"), mysql);
    for (const auto &[name, v] : groups) {
        if (name.startsWith(u"client") && name != u"client")
            make(name, v);
    }
    return bundle;
}

Result<QList<SessionInfo>> applyImport(SessionStore &store, SecretStore *secrets,
                                       const ImportBundle &bundle, ConflictPolicy policy)
{
    QHash<QString, QString> folderIds; // imported id → stored id
    for (const FolderInfo &f : bundle.folders) {
        if (!f.id.isEmpty() && store.folder(f.id)) {
            folderIds.insert(f.id, f.id); // already there: reuse
            continue;
        }
        const QString parent = folderIds.value(f.parentId);
        Result<FolderInfo> added = store.addFolder(f.name, parent);
        if (!added)
            return fail(added.error());
        folderIds.insert(f.id, added->id);
    }

    const bool keychain = secrets && secrets->isAvailable();
    QList<SessionInfo> written;
    for (const ImportedSession &imported : bundle.sessions) {
        SessionInfo s = imported.info;
        s.folderId = folderIds.value(s.folderId);
        if (!s.id.isEmpty() && store.session(s.id)) {
            if (policy == ConflictPolicy::Skip)
                continue;
            if (policy == ConflictPolicy::KeepBoth)
                s.id.clear();
        }
        if (!imported.password.isEmpty())
            s.secretModes.insert(secret::Password,
                                 keychain ? SecretMode::Keychain : SecretMode::Ask);
        Result<SessionInfo> saved = store.saveSession(s);
        if (!saved)
            return fail(saved.error());
        if (keychain && !imported.password.isEmpty())
            secrets->write(saved->id, secret::Password, imported.password);
        written.append(*saved);
    }
    return written;
}

} // namespace owl::app
