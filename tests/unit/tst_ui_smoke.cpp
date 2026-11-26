// SPDX-License-Identifier: GPL-3.0-or-later
// Loads the QML windows offscreen in both themes and fails on any QML warning.
// Set OWL_SCREENSHOT_DIR to also save <window>-<theme>.png screenshots there.
#include "appcontext.h"
#include "commandregistry.h"
#include "iconprovider.h"
#include "settings.h"
#include "thememanager.h"
#include "uistate.h"

#include <QDir>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTemporaryDir>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(OwlSql_ThemePlugin)
Q_IMPORT_QML_PLUGIN(OwlSql_StylePlugin)
Q_IMPORT_QML_PLUGIN(OwlSql_AppPlugin)
Q_IMPORT_QML_PLUGIN(OwlSql_UiPlugin)

using owl::theme::ThemeManager;

namespace {

void saveShot(QQuickWindow *window, const QString &name)
{
    const QString dir = qEnvironmentVariable("OWL_SCREENSHOT_DIR");
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QVERIFY(window->grabWindow().save(QDir(dir).filePath(name + QStringLiteral(".png"))));
}

} // namespace

class TestUiSmoke : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dataDir;
    std::unique_ptr<owl::app::AppContext> m_context;

private slots:
    void initTestCase()
    {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        QQuickStyle::setStyle(QStringLiteral("OwlSql.Style"));
        // Isolated settings and owlsql.db for this test run.
        m_context = std::make_unique<owl::app::AppContext>(
            owl::app::AppPaths::underRoot(m_dataDir.path()));
        QString error;
        QVERIFY2(m_context->initialize(&error), qPrintable(error));
    }

    void windowLoadsCleanly_data()
    {
        QTest::addColumn<QString>("component");
        QTest::addColumn<ThemeManager::Mode>("mode");
        for (const char *component : {"Main", "Gallery"}) {
            QTest::addRow("%s-light", component) << component << ThemeManager::Mode::Light;
            QTest::addRow("%s-dark", component) << component << ThemeManager::Mode::Dark;
        }
    }

    void windowLoadsCleanly()
    {
        QFETCH(QString, component);
        QFETCH(ThemeManager::Mode, mode);

        ThemeManager::instance()->setMode(mode);
        const QString shotName = component.toLower()
            + (mode == ThemeManager::Mode::Light ? QStringLiteral("-light")
                                                 : QStringLiteral("-dark"));

        QQmlApplicationEngine engine;
        owl::theme::IconProvider::install(engine);
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &errors) {
            for (const QQmlError &e : errors)
                warnings << e.toString();
        });
        engine.loadFromModule("OwlSql.Ui", component);

        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::qWait(150); // let async icon images arrive

        // QML must share the C++ instance, so the window follows the selected mode.
        auto *qmlTheme = engine.singletonInstance<ThemeManager *>("OwlSql.Theme", "Theme");
        QCOMPARE(qmlTheme, ThemeManager::instance());
        QCOMPARE(window->color(), ThemeManager::instance()->color()->bgCanvas());

        saveShot(window, shotName);

        // Gallery: open the menu and the dialog too, so popups are exercised.
        if (component == QLatin1String("Gallery")) {
            auto *menu = window->findChild<QObject *>(QStringLiteral("galleryMenu"));
            auto *dialog = window->findChild<QObject *>(QStringLiteral("galleryDialog"));
            QVERIFY(menu);
            QVERIFY(dialog);
            QVERIFY(QMetaObject::invokeMethod(menu, "popup", Q_ARG(QPointF, QPointF(40, 420))));
            QTest::qWait(250);
            saveShot(window, shotName + QStringLiteral("-menu"));
            QVERIFY(QMetaObject::invokeMethod(menu, "close"));
            QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
            QTest::qWait(250);
            saveShot(window, shotName + QStringLiteral("-dialog"));
        }

        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(u'\n')));
    }

    // The shell reacts to real key presses and to commands triggered through the registry.
    void shellShortcutsAndCommands()
    {
        ThemeManager::instance()->setMode(ThemeManager::Mode::Dark);
        QQmlApplicationEngine engine;
        owl::theme::IconProvider::install(engine);
        engine.loadFromModule("OwlSql.Ui", "Main");
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));

        QCOMPARE(window->property("bottomPanelVisible").toBool(), false);
        QTest::keyClick(window, Qt::Key_J, Qt::ControlModifier);
        QTRY_COMPARE(window->property("bottomPanelVisible").toBool(), true);
        QTest::keyClick(window, Qt::Key_J, Qt::ControlModifier);
        QTRY_COMPARE(window->property("bottomPanelVisible").toBool(), false);

        QCOMPARE(window->property("sidebarVisible").toBool(), true);
        QTest::keyClick(window, Qt::Key_B, Qt::ControlModifier);
        QTRY_COMPARE(window->property("sidebarVisible").toBool(), false);

        auto *commands = owl::app::CommandRegistry::instance();
        commands->trigger(QStringLiteral("view.toggleSidebar"));
        QCOMPARE(window->property("sidebarVisible").toBool(), true);

        QCOMPARE(window->property("tabCount").toInt(), 1);
        commands->trigger(QStringLiteral("workspace.closeTab"));
        QCOMPARE(window->property("tabCount").toInt(), 0);
        commands->trigger(QStringLiteral("workspace.showWelcome"));
        QCOMPARE(window->property("tabCount").toInt(), 1);
        commands->trigger(QStringLiteral("workspace.showWelcome")); // no duplicate tab
        QCOMPARE(window->property("tabCount").toInt(), 1);

        commands->trigger(QStringLiteral("view.toggleBottomPanel"));
        QTest::qWait(150);
        saveShot(window, QStringLiteral("main-dark-panel"));
    }

    // Layout survives a restart: UiState writes to owlsql.db and the shell restores from it.
    void layoutIsRestoredAfterRestart()
    {
        {
            QQmlApplicationEngine engine;
            owl::theme::IconProvider::install(engine);
            engine.loadFromModule("OwlSql.Ui", "Main");
            QObject *window = engine.rootObjects().value(0);
            QVERIFY(window);
            window->setProperty("bottomPanelVisible", true);
            window->setProperty("sidebarVisible", false);
            owl::app::CommandRegistry::instance()->trigger(QStringLiteral("workspace.closeTab"));
            QCOMPARE(window->property("tabCount").toInt(), 0);
        }
        m_context->uiState()->flush();

        // A fresh UiState reads back from the database, like a new process would.
        owl::app::UiState reloaded(m_context->database(), nullptr);
        QCOMPARE(reloaded.value(QStringLiteral("layout.bottomPanelVisible")).toBool(), true);
        QCOMPARE(reloaded.value(QStringLiteral("layout.sidebarVisible")).toBool(), false);
        QCOMPARE(reloaded.value(QStringLiteral("workspace.tabs")).toList().size(), 0);

        QQmlApplicationEngine engine;
        owl::theme::IconProvider::install(engine);
        engine.loadFromModule("OwlSql.Ui", "Main");
        QObject *window = engine.rootObjects().value(0);
        QVERIFY(window);
        QCOMPARE(window->property("bottomPanelVisible").toBool(), true);
        QCOMPARE(window->property("sidebarVisible").toBool(), false);
        QCOMPARE(window->property("tabCount").toInt(), 0);
    }

    // Theme commands go through settings.json, so the choice is remembered.
    void themeCommandsPersistToSettings()
    {
        auto *commands = owl::app::CommandRegistry::instance();
        commands->trigger(QStringLiteral("view.themeLight")); // no window needed: handled by QML
        QQmlApplicationEngine engine;
        owl::theme::IconProvider::install(engine);
        engine.loadFromModule("OwlSql.Ui", "Main");
        QVERIFY(!engine.rootObjects().isEmpty());
        commands->trigger(QStringLiteral("view.themeDark"));
        QCOMPARE(ThemeManager::instance()->mode(), ThemeManager::Mode::Dark);
        QCOMPARE(m_context->settings()->value(QStringLiteral("theme.mode")).toString(),
                 QStringLiteral("dark"));
    }

    void cleanupTestCase()
    {
        ThemeManager::instance()->setMode(ThemeManager::Mode::System);
        m_context.reset();
    }
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
