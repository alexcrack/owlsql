// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/driver.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace owl::app {

// Per-session safety level (decisions 015, 063).
enum class SafetyLevel : quint8 { Dev, Staging, Prod };
QString safetyLevelName(SafetyLevel level); // "dev" / "staging" / "prod"
SafetyLevel safetyLevelFromName(QStringView name); // unknown → Dev

// Where a secret comes from when connecting (app-store.md §2).
enum class SecretMode : quint8 { Keychain, Ask, None };
QString secretModeName(SecretMode mode);
SecretMode secretModeFromName(QStringView name); // unknown → Ask

// The secrets a session can have, as used in keychain keys and secret_modes.
namespace secret {
inline constexpr QLatin1StringView Password("password");
inline constexpr QLatin1StringView SshPassword("ssh-password");
inline constexpr QLatin1StringView SshKeyPassphrase("ssh-key-passphrase");
inline constexpr QLatin1StringView SslKeyPassphrase("ssl-key-passphrase");
} // namespace secret

// session.options (app-store.md §3.1, decision 088). Unset values fall back to settings.json.
struct SessionOptions
{
    QString startupScript; // runs on every new physical connection of the session
    QString defaultDatabase;
    QString defaultSchema;
    QStringList visibleDatabases; // empty = all
    std::optional<int> fetchSize;
    std::optional<int> queryTimeoutSec;
    std::optional<int> keepaliveMin;
    bool singleConnection = false; // no per-tab / per-database connections (decision 043)
    bool readOnly = false;

    bool operator==(const SessionOptions &) const = default;

    QJsonObject toJson() const; // only the keys that are set
    static SessionOptions fromJson(const QJsonObject &json);
};

struct FolderInfo
{
    QString id; // uuid
    QString parentId; // empty = top level
    QString name;
    int position = 0;
    bool expanded = true;

    bool operator==(const FolderInfo &) const = default;
};

// A saved session: everything needed to connect, except secrets.
struct SessionInfo
{
    QString id; // uuid; also the keychain key
    QString folderId;
    int position = 0;
    QString name;
    QString driverId;
    ConnectionParams params; // params.password is always empty here
    SafetyLevel safetyLevel = SafetyLevel::Dev;
    QString color; // tag colour name; empty = the level's colour
    SessionOptions options;
    QString notes;
    QHash<QString, SecretMode> secretModes; // secret::* → mode; missing = None
    QDateTime createdAt;
    QDateTime updatedAt;
    QDateTime lastUsedAt;
    QString importedFrom; // "json", "url", "pgpass", "mycnf", later "heidisql"…

    SecretMode secretMode(QLatin1StringView secret) const
    {
        return secretModes.value(secret, SecretMode::None);
    }
    bool operator==(const SessionInfo &) const = default;
};

QJsonObject secretModesToJson(const QHash<QString, SecretMode> &modes);
QHash<QString, SecretMode> secretModesFromJson(const QJsonObject &json);

// A new uuid in the form used for ids ("{…}" without braces).
QString newId();

} // namespace owl::app
