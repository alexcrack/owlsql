// SPDX-License-Identifier: GPL-3.0-or-later
#include "commandregistry.h"

#include <QSet>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using owl::app::CommandRegistry;

class TestCommands : public QObject
{
    Q_OBJECT

private slots:
    void builtinCommandsAreWellFormed()
    {
        QSet<QString> ids;
        QSet<QKeySequence> shortcuts;
        for (const CommandRegistry::Command &command : CommandRegistry::builtinCommands()) {
            QVERIFY2(!ids.contains(command.id), qPrintable(u"duplicate id "_s + command.id));
            ids.insert(command.id);
            QVERIFY2(command.id.contains(u'.'), qPrintable(command.id));
            QVERIFY2(!command.title.isEmpty(), qPrintable(command.id));
            QVERIFY2(!command.category.isEmpty(), qPrintable(command.id));
            if (!command.defaultShortcut.isEmpty()) {
                QVERIFY2(!shortcuts.contains(command.defaultShortcut),
                         qPrintable(u"shortcut clash on "_s + command.id));
                shortcuts.insert(command.defaultShortcut);
            }
        }
    }

    void lookupAndTrigger()
    {
        CommandRegistry registry(CommandRegistry::builtinCommands(), nullptr);
        QVERIFY(registry.contains(u"view.toggleBottomPanel"_s));
        QCOMPARE(registry.title(u"view.toggleBottomPanel"_s), u"Toggle Bottom Panel"_s);
        QCOMPARE(registry.shortcut(u"view.toggleBottomPanel"_s), u"Ctrl+J"_s);

        QSignalSpy spy(&registry, &CommandRegistry::triggered);
        registry.trigger(u"view.toggleSidebar"_s);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), u"view.toggleSidebar"_s);
    }

    void unknownCommandsAreIgnored()
    {
        CommandRegistry registry(CommandRegistry::builtinCommands(), nullptr);
        QSignalSpy spy(&registry, &CommandRegistry::triggered);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(u"Unknown command"_s));
        registry.trigger(u"no.such.command"_s);
        QCOMPARE(spy.count(), 0);
        QVERIFY(registry.title(u"no.such.command"_s).isEmpty());
        QVERIFY(registry.shortcut(u"no.such.command"_s).isEmpty());
    }

    void shortcutOverrides()
    {
        CommandRegistry registry(CommandRegistry::builtinCommands(), nullptr);
        QSignalSpy changed(&registry, &CommandRegistry::changed);
        const int revision = registry.revision();

        registry.setShortcutOverride(u"view.toggleBottomPanel"_s, QKeySequence(u"Ctrl+Alt+J"_s));
        QCOMPARE(registry.shortcut(u"view.toggleBottomPanel"_s), u"Ctrl+Alt+J"_s);
        QCOMPARE(changed.count(), 1);
        QVERIFY(registry.revision() > revision);

        registry.setShortcutOverride(u"view.toggleBottomPanel"_s, QKeySequence());
        QVERIFY(registry.shortcut(u"view.toggleBottomPanel"_s).isEmpty());

        registry.clearShortcutOverrides();
        QCOMPARE(registry.shortcut(u"view.toggleBottomPanel"_s), u"Ctrl+J"_s);
        QCOMPARE(registry.shortcuts().value(u"view.toggleBottomPanel"_s).toString(), u"Ctrl+J"_s);
        QCOMPARE(registry.titles().value(u"help.about"_s).toString(), u"About OwlSql"_s);
        QCOMPARE(changed.count(), 3);
    }
};

QTEST_MAIN(TestCommands)
#include "tst_commands.moc"
