// SPDX-License-Identifier: GPL-3.0-or-later
// settings.json and keybindings.json.
#include "commandregistry.h"
#include "keybindings.h"
#include "settings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using owl::app::CommandRegistry;
using owl::app::Keybindings;
using owl::app::Settings;

namespace {

void writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()
                                          : QJsonObject();
}

} // namespace

class TestSettings : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString settingsPath() const { return m_dir.filePath(u"settings.json"_s); }
    QString keysPath() const { return m_dir.filePath(u"keybindings.json"_s); }

private slots:
    void init()
    {
        QFile::remove(settingsPath());
        QFile::remove(keysPath());
    }

    void defaultsWithoutFile()
    {
        Settings settings(settingsPath(), nullptr);
        QCOMPARE(settings.value(u"theme.mode"_s).toString(), u"system"_s);
        QCOMPARE(settings.value(u"ui.reduceMotion"_s).toBool(), false);
        QVERIFY(settings.problems().isEmpty());
    }

    void writesOnlyNonDefaults()
    {
        Settings settings(settingsPath(), nullptr);
        QSignalSpy spy(&settings, &Settings::valueChanged);
        QVERIFY(settings.setValue(u"theme.mode"_s, u"dark"_s));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(readJson(settingsPath()).value(u"theme.mode"_s).toString(), u"dark"_s);

        QVERIFY(settings.setValue(u"theme.mode"_s, u"system"_s)); // back to default
        QVERIFY(!readJson(settingsPath()).contains(u"theme.mode"_s));
    }

    void rejectsInvalidValuesAndUnknownKeys()
    {
        Settings settings(settingsPath(), nullptr);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Invalid value"_s));
        QVERIFY(!settings.setValue(u"theme.mode"_s, u"purple"_s));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Invalid value"_s));
        QVERIFY(!settings.setValue(u"ui.reduceMotion"_s, u"yes"_s));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Unknown setting"_s));
        QVERIFY(!settings.setValue(u"no.such.key"_s, 1));
        QCOMPARE(settings.value(u"theme.mode"_s).toString(), u"system"_s);
    }

    void reportsProblemsAndKeepsUnknownKeys()
    {
        writeFile(settingsPath(), R"({ "theme.mode": "purple", "future.option": 3 })");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"theme.mode: invalid value"_s));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"future.option: unknown"_s));
        Settings settings(settingsPath(), nullptr);
        QCOMPARE(settings.value(u"theme.mode"_s).toString(), u"system"_s); // fell back
        QCOMPARE(settings.problems().size(), 2);

        QVERIFY(settings.setValue(u"ui.reduceMotion"_s, true));
        const QJsonObject saved = readJson(settingsPath());
        QCOMPARE(saved.value(u"future.option"_s).toInt(), 3); // unknown key preserved
        QVERIFY(saved.value(u"ui.reduceMotion"_s).toBool());
    }

    void malformedFileFallsBackToDefaults()
    {
        writeFile(settingsPath(), "{ not json");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"not a valid JSON object"_s));
        Settings settings(settingsPath(), nullptr);
        QCOMPARE(settings.value(u"theme.mode"_s).toString(), u"system"_s);
        QCOMPARE(settings.problems().size(), 1);
    }

    void appliesOutsideEditsLive()
    {
        Settings settings(settingsPath(), nullptr);
        QVERIFY(settings.setValue(u"ui.reduceMotion"_s, true)); // creates the file
        QSignalSpy spy(&settings, &Settings::valueChanged);

        writeFile(settingsPath(), R"({ "theme.mode": "light" })");
        QTRY_COMPARE_WITH_TIMEOUT(settings.value(u"theme.mode"_s).toString(), u"light"_s, 5000);
        QCOMPARE(settings.value(u"ui.reduceMotion"_s).toBool(), false); // removed from file
        QVERIFY(spy.count() >= 2);
    }

    void keybindingOverrides()
    {
        writeFile(keysPath(), R"([
            { "command": "view.toggleBottomPanel", "key": "Ctrl+Alt+J" },
            { "command": "view.toggleTheme", "key": "" },
            { "command": "no.such.command", "key": "Ctrl+1" },
            { "command": "view.toggleSidebar", "key": "NotAKey++" }
        ])");
        CommandRegistry registry(CommandRegistry::builtinCommands(), nullptr);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"no.such.command: unknown"_s));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(u"view.toggleSidebar: invalid key"_s));
        Keybindings keybindings(keysPath(), &registry, nullptr);

        QCOMPARE(registry.shortcut(u"view.toggleBottomPanel"_s), u"Ctrl+Alt+J"_s);
        QVERIFY(registry.shortcut(u"view.toggleTheme"_s).isEmpty());
        QCOMPARE(registry.shortcut(u"view.toggleSidebar"_s), u"Ctrl+B"_s); // invalid -> default
        QCOMPARE(keybindings.problems().size(), 2);

        // Removing the file's entries restores defaults on reload.
        writeFile(keysPath(), "[]");
        QTRY_COMPARE_WITH_TIMEOUT(registry.shortcut(u"view.toggleBottomPanel"_s), u"Ctrl+J"_s,
                                  5000);
        QCOMPARE(registry.shortcut(u"view.toggleTheme"_s), u"Ctrl+Shift+T"_s);
    }
};

// GUI application: standard key sequences (CommandRegistry) come from the platform theme.
QTEST_MAIN(TestSettings)
#include "tst_settings.moc"
