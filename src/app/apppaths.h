// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace owl::app {

// Where OwlSql keeps its own files (docs/specs/app-store.md §1). Everything goes through here so
// a portable mode can be added later (decision 072).
struct AppPaths
{
    QString configDir; // settings.json, keybindings.json
    QString dataDir; // owlsql.db, backups/

    // QStandardPaths AppConfigLocation / AppDataLocation.
    static AppPaths standard();
    // <root>/config and <root>/data — used by --data-dir and tests.
    static AppPaths underRoot(const QString &root);

    QString settingsFile() const;
    QString keybindingsFile() const;
    QString appDatabaseFile() const;
    QString backupsDir() const;

    // Creates the directories; returns false if that fails.
    bool ensureExists() const;
};

} // namespace owl::app
