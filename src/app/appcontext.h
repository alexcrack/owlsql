// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "apppaths.h"

#include <QObject>

#include <memory>

namespace owl::app {

class AppDatabase;
class Keybindings;
class Settings;
class UiState;

// Owns the application services that QML singletons expose (Settings, UiState, ...) and wires
// them together (settings -> theme, keybindings -> commands). Create one in main() before any
// QML is loaded; tests create their own over a temporary directory.
class AppContext : public QObject
{
    Q_OBJECT

public:
    explicit AppContext(AppPaths paths, QObject *parent = nullptr);
    ~AppContext() override;

    // Creates directories, opens and migrates owlsql.db, loads settings and keybindings.
    // Returns false with *error when the app must not continue (e.g. database from a newer
    // OwlSql). Other problems (unwritable dirs) degrade to in-memory state with a warning.
    bool initialize(QString *error);

    static AppContext *current();

    const AppPaths &paths() const { return m_paths; }
    Settings *settings() const { return m_settings; }
    UiState *uiState() const { return m_uiState; }
    Keybindings *keybindings() const { return m_keybindings; }
    AppDatabase *database() const { return m_database.get(); }

private:
    void applyThemeSettings();

    AppPaths m_paths;
    std::unique_ptr<AppDatabase> m_database;
    Settings *m_settings = nullptr;
    UiState *m_uiState = nullptr;
    Keybindings *m_keybindings = nullptr;
};

} // namespace owl::app
