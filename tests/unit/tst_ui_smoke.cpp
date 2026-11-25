// SPDX-License-Identifier: GPL-3.0-or-later
// Loads the main QML window offscreen in both themes and fails on any QML warning.
// Set OWL_SCREENSHOT_DIR to also save main-light.png / main-dark.png there.
#include "iconprovider.h"
#include "thememanager.h"

#include <QDir>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(OwlSql_ThemePlugin)
Q_IMPORT_QML_PLUGIN(OwlSql_UiPlugin)

using owl::theme::ThemeManager;

class TestUiSmoke : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        QQuickStyle::setStyle(QStringLiteral("Basic"));
    }

    void mainWindowLoadsCleanly_data()
    {
        QTest::addColumn<ThemeManager::Mode>("mode");
        QTest::addColumn<QString>("shotName");
        QTest::newRow("light") << ThemeManager::Mode::Light << QStringLiteral("main-light.png");
        QTest::newRow("dark") << ThemeManager::Mode::Dark << QStringLiteral("main-dark.png");
    }

    void mainWindowLoadsCleanly()
    {
        QFETCH(ThemeManager::Mode, mode);
        QFETCH(QString, shotName);

        ThemeManager::instance()->setMode(mode);

        QQmlApplicationEngine engine;
        owl::theme::IconProvider::install(engine);
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&warnings](const QList<QQmlError> &errors) {
            for (const QQmlError &e : errors)
                warnings << e.toString();
        });
        engine.loadFromModule("OwlSql.Ui", "Main");

        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        QTest::qWait(100); // let async icon images arrive

        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(u'\n')));

        // QML must share the C++ instance, so the window follows the selected mode.
        auto *qmlTheme = engine.singletonInstance<ThemeManager *>("OwlSql.Theme", "Theme");
        QCOMPARE(qmlTheme, ThemeManager::instance());
        QCOMPARE(window->color(), ThemeManager::instance()->color()->bgCanvas());

        const QImage shot = window->grabWindow();
        QVERIFY(!shot.isNull());
        if (const QString dir = qEnvironmentVariable("OWL_SCREENSHOT_DIR"); !dir.isEmpty()) {
            QDir().mkpath(dir);
            QVERIFY(shot.save(QDir(dir).filePath(shotName)));
        }
    }

    void cleanupTestCase() { ThemeManager::instance()->setMode(ThemeManager::Mode::System); }
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
