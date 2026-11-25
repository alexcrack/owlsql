// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QVariantMap>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace owl::app {

// Every user-invokable action, identified by a stable id ("view.toggleBottomPanel").
// Menus, shortcuts, the command palette (M3) and keybindings.json overrides all go through it
// (decision 081). QML: CommandAction { command: "view.toggleSidebar" } and
// Connections { target: Commands; function onTriggered(id) { ... } }.
class CommandRegistry : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Commands)
    QML_SINGLETON

    // Bumped whenever titles or shortcuts change.
    Q_PROPERTY(int revision READ revision NOTIFY changed FINAL)
    // id -> value maps for QML bindings that must follow overrides:
    // text: Commands.titles[command]; shortcut: Commands.shortcuts[command]
    Q_PROPERTY(QVariantMap titles READ titles NOTIFY changed FINAL)
    Q_PROPERTY(QVariantMap shortcuts READ shortcuts NOTIFY changed FINAL)
    Q_PROPERTY(QVariantMap shortcutTexts READ shortcutTexts NOTIFY changed FINAL)

public:
    struct Command
    {
        QString id;
        QString title;
        QString category;
        QKeySequence defaultShortcut;
    };

    // Deliberately not default-constructible (see ThemeManager): QML must use create().
    CommandRegistry(QList<Command> commands, QObject *parent);

    static CommandRegistry *instance();
    static CommandRegistry *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    // The commands OwlSql ships with.
    static QList<Command> builtinCommands();

    int revision() const { return m_revision; }
    QList<Command> commands() const { return m_commands; }
    QVariantMap titles() const;
    QVariantMap shortcuts() const;
    QVariantMap shortcutTexts() const;

    Q_INVOKABLE bool contains(const QString &id) const;
    Q_INVOKABLE QString title(const QString &id) const;
    // Portable text ("Ctrl+J") for Action/Shortcut; Ctrl means Cmd on macOS.
    Q_INVOKABLE QString shortcut(const QString &id) const;
    // Text for display in the UI, native to the platform ("⌘J" on macOS).
    Q_INVOKABLE QString shortcutText(const QString &id) const;
    Q_INVOKABLE void trigger(const QString &id);

    QKeySequence effectiveShortcut(const QString &id) const;
    // keybindings.json support: an empty sequence removes the shortcut.
    void setShortcutOverride(const QString &id, const QKeySequence &sequence);
    void clearShortcutOverrides();

signals:
    void triggered(const QString &id);
    void changed();

private:
    const Command *find(const QString &id) const;

    QList<Command> m_commands;
    QHash<QString, qsizetype> m_index;
    QHash<QString, QKeySequence> m_overrides;
    int m_revision = 0;
};

} // namespace owl::app
