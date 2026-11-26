// SPDX-License-Identifier: GPL-3.0-or-later
#include "appcontext.h"

#include "appdatabase.h"
#include "commandregistry.h"
#include "keybindings.h"
#include "settings.h"
#include "thememanager.h"
#include "uistate.h"

#include <QCoreApplication>
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
{
    Q_ASSERT_X(!s_current, "AppContext", "only one AppContext may exist at a time");
    s_current = this;
}

AppContext::~AppContext()
{
    delete m_uiState; // flushes into the database before it closes
    m_uiState = nullptr;
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

    connect(m_settings, &Settings::valueChanged, this, &AppContext::applyThemeSettings);
    applyThemeSettings();

    if (auto *app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, m_uiState, &UiState::flush);
    return true;
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
