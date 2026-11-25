// SPDX-License-Identifier: GPL-3.0-or-later
#include "iconprovider.h"
#include "thememanager.h"

#include <QFontDatabase>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>

using owl::theme::IconProvider;
using owl::theme::ThemeManager;

class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    void tokenKeyToName_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("name");
        QTest::newRow("single") << "danger" << "danger";
        QTest::newRow("two parts") << "bg.canvas" << "bgCanvas";
        QTest::newRow("camel tail") << "bg.selectedInactive" << "bgSelectedInactive";
        QTest::newRow("subtle") << "danger.subtle" << "dangerSubtle";
    }

    void tokenKeyToName()
    {
        QFETCH(QString, key);
        QFETCH(QString, name);
        QCOMPARE(ThemeManager::tokenKeyToName(key), name);
    }

    // Every bundled file defines exactly the tokens the code knows — in both themes.
    void bundledTokenFilesAreComplete()
    {
        ThemeManager theme(ThemeManager::Paths{}, nullptr);
        theme.setMode(ThemeManager::Mode::Light);
        theme.setMode(ThemeManager::Mode::Dark);
        QVERIFY2(theme.loadErrors().isEmpty(), qPrintable(theme.loadErrors().join(u'\n')));
    }

    void metricsMatchDesignSpec()
    {
        ThemeManager theme(ThemeManager::Paths{}, nullptr);
        QCOMPARE(theme.size()->row(), 24);
        QCOMPARE(theme.size()->control(), 28);
        QCOMPARE(theme.radius()->control(), 4);
        QCOMPARE(theme.space()->xxl(), 24);
        QCOMPARE(theme.font()->ui().family(), QStringLiteral("Inter"));
        QCOMPARE(theme.font()->ui().pixelSize(), 13);
        QCOMPARE(theme.font()->code().family(), QStringLiteral("JetBrains Mono"));
    }

    void explicitModesSwitchPalettes()
    {
        ThemeManager theme(ThemeManager::Paths{}, nullptr);
        QSignalSpy themeSpy(&theme, &ThemeManager::themeChanged);
        QSignalSpy colorSpy(theme.color(), &owl::theme::ThemeColors::changed);

        theme.setMode(ThemeManager::Mode::Dark);
        const QColor darkCanvas = theme.color()->bgCanvas();
        QVERIFY(theme.isDark());
        QCOMPARE(theme.name(), QStringLiteral("Owl Dark"));

        theme.setMode(ThemeManager::Mode::Light);
        QVERIFY(!theme.isDark());
        QCOMPARE(theme.name(), QStringLiteral("Owl Light"));
        QCOMPARE(theme.color()->bgCanvas(), QColor(QStringLiteral("#F2F0EC")));
        QVERIFY(theme.color()->bgCanvas() != darkCanvas);
        QVERIFY(themeSpy.count() >= 1);
        QVERIFY(colorSpy.count() >= 1);

        theme.setMode(ThemeManager::Mode::System); // restore global color scheme for other tests
    }

    void reduceMotionZeroesDurations()
    {
        ThemeManager theme(ThemeManager::Paths{}, nullptr);
        QCOMPARE(theme.motion()->fast(), 100);
        theme.setReduceMotion(true);
        QCOMPARE(theme.motion()->fast(), 0);
        QCOMPARE(theme.motion()->normal(), 0);
        theme.setReduceMotion(false);
        QCOMPARE(theme.motion()->normal(), 150);
    }

    void reportsBrokenThemeFiles()
    {
        ThemeManager::Paths paths;
        paths.metrics = QStringLiteral(":/does/not/exist.json");
        ThemeManager theme(paths, nullptr);
        QVERIFY(!theme.loadErrors().isEmpty());
    }

    void fontsAreRegistered()
    {
        ThemeManager::registerFonts();
        const QStringList families = QFontDatabase::families();
        QVERIFY(families.contains(QStringLiteral("Inter")));
        QVERIFY(families.contains(QStringLiteral("JetBrains Mono")));
    }

    void iconProviderTintsIcons()
    {
        IconProvider provider;
        QSize size;
        const QImage image
            = provider.requestImage(QStringLiteral("table?color=%23ff0000"), &size, QSize(32, 32));
        QCOMPARE(size, QSize(32, 32));
        QVERIFY(!image.isNull());

        bool foundRed = false;
        for (int y = 0; y < image.height() && !foundRed; ++y) {
            for (int x = 0; x < image.width() && !foundRed; ++x) {
                const QColor c = image.pixelColor(x, y);
                foundRed = c.alpha() > 200 && c.red() > 200 && c.green() < 40 && c.blue() < 40;
            }
        }
        QVERIFY2(foundRed, "expected opaque red stroke pixels");
    }

    void iconProviderRejectsBadNames()
    {
        IconProvider provider;
        QSize size;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Invalid icon name")));
        QVERIFY(provider.requestImage(QStringLiteral("../themes/owl-dark"), &size, {}).isNull());
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Unknown icon")));
        QVERIFY(provider.requestImage(QStringLiteral("no-such-icon"), &size, {}).isNull());
    }
};

QTEST_MAIN(TestTheme)
#include "tst_theme.moc"
