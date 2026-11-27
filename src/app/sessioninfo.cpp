// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessioninfo.h"

#include <QJsonArray>
#include <QUuid>

namespace owl::app {

QString safetyLevelName(SafetyLevel level)
{
    switch (level) {
    case SafetyLevel::Staging:
        return QStringLiteral("staging");
    case SafetyLevel::Prod:
        return QStringLiteral("prod");
    case SafetyLevel::Dev:
        break;
    }
    return QStringLiteral("dev");
}

SafetyLevel safetyLevelFromName(QStringView name)
{
    if (name == u"staging")
        return SafetyLevel::Staging;
    if (name == u"prod")
        return SafetyLevel::Prod;
    return SafetyLevel::Dev;
}

QString secretModeName(SecretMode mode)
{
    switch (mode) {
    case SecretMode::Keychain:
        return QStringLiteral("keychain");
    case SecretMode::None:
        return QStringLiteral("none");
    case SecretMode::Ask:
        break;
    }
    return QStringLiteral("ask");
}

SecretMode secretModeFromName(QStringView name)
{
    if (name == u"keychain")
        return SecretMode::Keychain;
    if (name == u"none")
        return SecretMode::None;
    return SecretMode::Ask;
}

QJsonObject SessionOptions::toJson() const
{
    QJsonObject o;
    const auto text = [&o](const char *key, const QString &value) {
        if (!value.isEmpty())
            o.insert(QLatin1StringView(key), value);
    };
    const auto number = [&o](const char *key, const std::optional<int> &value) {
        if (value)
            o.insert(QLatin1StringView(key), *value);
    };
    text("startupScript", startupScript);
    text("defaultDatabase", defaultDatabase);
    text("defaultSchema", defaultSchema);
    if (!visibleDatabases.isEmpty())
        o.insert(QStringLiteral("visibleDatabases"), QJsonArray::fromStringList(visibleDatabases));
    number("fetchSize", fetchSize);
    number("queryTimeoutSec", queryTimeoutSec);
    number("keepaliveMin", keepaliveMin);
    if (singleConnection)
        o.insert(QStringLiteral("singleConnection"), true);
    if (readOnly)
        o.insert(QStringLiteral("readOnly"), true);
    return o;
}

SessionOptions SessionOptions::fromJson(const QJsonObject &o)
{
    SessionOptions s;
    const auto number = [&o](QStringView key) -> std::optional<int> {
        const QJsonValue v = o.value(key);
        return v.isDouble() ? std::optional(v.toInt()) : std::nullopt;
    };
    s.startupScript = o.value(u"startupScript").toString();
    s.defaultDatabase = o.value(u"defaultDatabase").toString();
    s.defaultSchema = o.value(u"defaultSchema").toString();
    for (const auto &v : o.value(u"visibleDatabases").toArray())
        s.visibleDatabases << v.toString();
    s.fetchSize = number(u"fetchSize");
    s.queryTimeoutSec = number(u"queryTimeoutSec");
    s.keepaliveMin = number(u"keepaliveMin");
    s.singleConnection = o.value(u"singleConnection").toBool();
    s.readOnly = o.value(u"readOnly").toBool();
    return s;
}

QJsonObject secretModesToJson(const QHash<QString, SecretMode> &modes)
{
    QJsonObject o;
    for (auto it = modes.cbegin(); it != modes.cend(); ++it)
        o.insert(it.key(), secretModeName(it.value()));
    return o;
}

QHash<QString, SecretMode> secretModesFromJson(const QJsonObject &json)
{
    QHash<QString, SecretMode> modes;
    for (auto it = json.constBegin(); it != json.constEnd(); ++it)
        modes.insert(it.key(), secretModeFromName(it.value().toString()));
    return modes;
}

QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

} // namespace owl::app
