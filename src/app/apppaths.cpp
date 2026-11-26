// SPDX-License-Identifier: GPL-3.0-or-later
#include "apppaths.h"

#include <QDir>
#include <QStandardPaths>

namespace owl::app {

AppPaths AppPaths::standard()
{
    return {.configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation),
            .dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)};
}

AppPaths AppPaths::underRoot(const QString &root)
{
    const QDir dir(root);
    return {.configDir = dir.absoluteFilePath(QStringLiteral("config")),
            .dataDir = dir.absoluteFilePath(QStringLiteral("data"))};
}

QString AppPaths::settingsFile() const
{
    return QDir(configDir).filePath(QStringLiteral("settings.json"));
}

QString AppPaths::keybindingsFile() const
{
    return QDir(configDir).filePath(QStringLiteral("keybindings.json"));
}

QString AppPaths::appDatabaseFile() const
{
    return QDir(dataDir).filePath(QStringLiteral("owlsql.db"));
}

QString AppPaths::backupsDir() const
{
    return QDir(dataDir).filePath(QStringLiteral("backups"));
}

bool AppPaths::ensureExists() const
{
    return QDir().mkpath(configDir) && QDir().mkpath(dataDir) && QDir().mkpath(backupsDir());
}

} // namespace owl::app
