// SPDX-License-Identifier: GPL-3.0-or-later
#include "appcontext.h"

#include "appdatabase.h"
#include "commandregistry.h"
#include "keybindings.h"
#include "owl/core/driverregistry.h"
#include "secretstore.h"
#include "sessionmanager.h"
#include "sessionstore.h"
#include "settings.h"
#include "thememanager.h"
#include "uistate.h"

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>

Q_STATIC_LOGGING_CATEGORY(lcApp, "owlsql.app")

namespace owl::app {

namespace {
AppContext *s_current = nullptr;
}

AppContext::AppContext(AppPaths paths, QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_database(std::make_unique<AppDatabase>())
    , m_drivers(std::make_unique<DriverRegistry>())
{
    Q_ASSERT_X(!s_current, "AppContext", "only one AppContext may exist at a time");
    s_current = this;
}

AppContext::~AppContext()
{
    delete m_uiState; // flushes into the database before it closes
    m_uiState = nullptr;
    delete m_sessionManager; // closes open sessions before the drivers go
    m_sessionManager = nullptr;
    delete m_sessions;
    m_sessions = nullptr;
    m_database.reset();
    if (s_current == this)
        s_current = nullptr;
}

AppContext *AppContext::current()
{
    Q_ASSERT_X(s_current, "AppContext::current", "create an AppContext before loading QML");
    return s_current;
}

bool AppContext::initialize(QString *error)
{
    if (!m_paths.ensureExists())
        qCWarning(lcApp) << "Cannot create" << m_paths.configDir << "or" << m_paths.dataDir;

    QString dbError;
    if (!m_database->open(m_paths.appDatabaseFile(), m_paths.backupsDir(), &dbError)) {
        if (m_database->lastErrorKind() == AppDatabase::ErrorKind::NewerSchema) {
            *error = dbError;
            return false;
        }
        qCWarning(lcApp).noquote() << dbError << "- UI state will not be remembered";
    }

    m_settings = new Settings(m_paths.settingsFile(), this);
    m_uiState = new UiState(m_database->isOpen() ? m_database.get() : nullptr, this);
    m_keybindings = new Keybindings(m_paths.keybindingsFile(), CommandRegistry::instance(), this);
    m_secrets = new KeychainSecretStore(this);
    if (m_database->isOpen())
        m_sessions = new SessionStore(m_database->database(), this);
    else
        qCWarning(lcApp) << "Sessions are unavailable without owlsql.db";
    loadDrivers();
    m_sessionManager = new SessionManager(m_sessions, m_drivers.get(), m_secrets, this);

    connect(m_settings, &Settings::valueChanged, this, &AppContext::applyThemeSettings);
    applyThemeSettings();

    if (auto *app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, m_uiState, &UiState::flush);
    return true;
}

void AppContext::loadDrivers()
{
    QStringList dirs;
    if (const QString env = qEnvironmentVariable("OWL_DRIVER_DIR"); !env.isEmpty())
        dirs << env;
    else if (QCoreApplication::instance()) {
        const QDir app(QCoreApplication::applicationDirPath());
        dirs << app.filePath(QStringLiteral("drivers")) // Windows, macOS bundle
             << app.filePath(QStringLiteral("../drivers")) // build tree
             << app.filePath(QStringLiteral("../lib/owlsql/drivers")); // Linux install
    }
    for (const QString &dir : std::as_const(dirs)) {
        if (!QDir(dir).exists())
            continue;
        for (const QString &problem : m_drivers->loadPlugins(dir))
            qCWarning(lcApp).noquote() << problem;
    }
    qCInfo(lcApp).noquote() << "Drivers:" << m_drivers->ids().join(u", ");
}

void AppContext::applyThemeSettings()
{
    auto *theme = theme::ThemeManager::instance();
    const QString mode = m_settings->value(QStringLiteral("theme.mode")).toString();
    theme->setMode(mode == QLatin1String("light")      ? theme::ThemeManager::Mode::Light
                       : mode == QLatin1String("dark") ? theme::ThemeManager::Mode::Dark
                                                       : theme::ThemeManager::Mode::System);
    theme->setReduceMotion(m_settings->value(QStringLiteral("ui.reduceMotion")).toBool());
}

} // namespace owl::app
