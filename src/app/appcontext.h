// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "apppaths.h"

#include <QObject>

#include <memory>

namespace owl {
class DriverRegistry;
}

namespace owl::app {

class AppDatabase;
class SecretStore;
class SessionManager;
class SessionStore;
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
    // Database drivers (plugins from OWL_DRIVER_DIR, or next to / above the executable).
    DriverRegistry *drivers() const { return m_drivers.get(); }
    // Saved sessions; null when owlsql.db could not be opened.
    SessionStore *sessions() const { return m_sessions; }
    SecretStore *secrets() const { return m_secrets; }
    SessionManager *sessionManager() const { return m_sessionManager; }

private:
    void applyThemeSettings();
    void loadDrivers();

    AppPaths m_paths;
    std::unique_ptr<AppDatabase> m_database;
    Settings *m_settings = nullptr;
    UiState *m_uiState = nullptr;
    Keybindings *m_keybindings = nullptr;
    std::unique_ptr<DriverRegistry> m_drivers;
    SessionStore *m_sessions = nullptr;
    SecretStore *m_secrets = nullptr;
    SessionManager *m_sessionManager = nullptr;
};

} // namespace owl::app
