// SPDX-License-Identifier: GPL-3.0-or-later
// Loads the QML windows offscreen in both themes and fails on any QML warning.
// Set OWL_SCREENSHOT_DIR to also save <window>-<theme>.png screenshots there.
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
Q_IMPORT_QML_PLUGIN(OwlSql_StylePlugin)
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

private slots:
    void initTestCase()
    {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
        QQuickStyle::setStyle(QStringLiteral("OwlSql.Style"));
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

    void cleanupTestCase() { ThemeManager::instance()->setMode(ThemeManager::Mode::System); }
};

QTEST_MAIN(TestUiSmoke)
#include "tst_ui_smoke.moc"
