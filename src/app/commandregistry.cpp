// SPDX-License-Identifier: GPL-3.0-or-later
#include "commandregistry.h"

#include <QCoreApplication>
#include <QJSEngine>
#include <QLoggingCategory>

Q_STATIC_LOGGING_CATEGORY(lcCommands, "owlsql.commands")

namespace owl::app {

namespace {
CommandRegistry *s_instance = nullptr;
} // namespace

QList<CommandRegistry::Command> CommandRegistry::builtinCommands()
{
    const auto command = [](const char *id, const QString &title, const QString &category,
                            const QKeySequence &shortcut = {}) {
        return Command{.id = QString::fromLatin1(id),
                       .title = title,
                       .category = category,
                       .defaultShortcut = shortcut};
    };
    const auto keys = [](const char *portableText) {
        return QKeySequence(QString::fromLatin1(portableText), QKeySequence::PortableText);
    };

    const QString app = tr("Application");
    const QString view = tr("View");
    const QString workspace = tr("Workspace");
    const QString help = tr("Help");

    return {
        command("app.quit", tr("Quit OwlSql"), app, QKeySequence(QKeySequence::Quit)),
        command("view.toggleSidebar", tr("Toggle Sidebar"), view, keys("Ctrl+B")),
        command("view.toggleBottomPanel", tr("Toggle Bottom Panel"), view, keys("Ctrl+J")),
        command("view.toggleTheme", tr("Toggle Light/Dark Theme"), view, keys("Ctrl+Shift+T")),
        command("view.themeSystem", tr("Theme: Follow System"), view),
        command("view.themeLight", tr("Theme: Light"), view),
        command("view.themeDark", tr("Theme: Dark"), view),
        command("workspace.showWelcome", tr("Show Welcome Tab"), workspace),
        command("workspace.closeTab", tr("Close Tab"), workspace, keys("Ctrl+W")),
        command("workspace.nextTab", tr("Next Tab"), workspace, keys("Ctrl+Tab")),
        command("workspace.previousTab", tr("Previous Tab"), workspace, keys("Ctrl+Shift+Tab")),
        command("help.about", tr("About OwlSql"), help),
    };
}

CommandRegistry::CommandRegistry(QList<Command> commands, QObject *parent)
    : QObject(parent)
    , m_commands(std::move(commands))
{
    for (qsizetype i = 0; i < m_commands.size(); ++i) {
        const QString &id = m_commands.at(i).id;
        if (m_index.contains(id))
            qCWarning(lcCommands) << "Duplicate command id" << id;
        m_index.insert(id, i);
    }
}

CommandRegistry *CommandRegistry::instance()
{
    if (!s_instance)
        s_instance = new CommandRegistry(builtinCommands(), QCoreApplication::instance());
    return s_instance;
}

CommandRegistry *CommandRegistry::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    CommandRegistry *registry = instance();
    QJSEngine::setObjectOwnership(registry, QJSEngine::CppOwnership);
    return registry;
}

const CommandRegistry::Command *CommandRegistry::find(const QString &id) const
{
    const auto it = m_index.constFind(id);
    return it == m_index.constEnd() ? nullptr : &m_commands.at(*it);
}

bool CommandRegistry::contains(const QString &id) const
{
    return find(id) != nullptr;
}

QString CommandRegistry::title(const QString &id) const
{
    const Command *command = find(id);
    return command ? command->title : QString();
}

QKeySequence CommandRegistry::effectiveShortcut(const QString &id) const
{
    if (const auto it = m_overrides.constFind(id); it != m_overrides.constEnd())
        return *it;
    const Command *command = find(id);
    return command ? command->defaultShortcut : QKeySequence();
}

QString CommandRegistry::shortcut(const QString &id) const
{
    return effectiveShortcut(id).toString(QKeySequence::PortableText);
}

QString CommandRegistry::shortcutText(const QString &id) const
{
    return effectiveShortcut(id).toString(QKeySequence::NativeText);
}

QVariantMap CommandRegistry::titles() const
{
    QVariantMap map;
    for (const Command &command : m_commands)
        map.insert(command.id, command.title);
    return map;
}

QVariantMap CommandRegistry::shortcuts() const
{
    QVariantMap map;
    for (const Command &command : m_commands)
        map.insert(command.id, shortcut(command.id));
    return map;
}

QVariantMap CommandRegistry::shortcutTexts() const
{
    QVariantMap map;
    for (const Command &command : m_commands)
        map.insert(command.id, shortcutText(command.id));
    return map;
}

void CommandRegistry::trigger(const QString &id)
{
    if (!find(id)) {
        qCWarning(lcCommands) << "Unknown command" << id;
        return;
    }
    emit triggered(id);
}

void CommandRegistry::setShortcutOverride(const QString &id, const QKeySequence &sequence)
{
    if (!find(id)) {
        qCWarning(lcCommands) << "Shortcut override for unknown command" << id;
        return;
    }
    m_overrides.insert(id, sequence);
    ++m_revision;
    emit changed();
}

void CommandRegistry::clearShortcutOverrides()
{
    if (m_overrides.isEmpty())
        return;
    m_overrides.clear();
    ++m_revision;
    emit changed();
}

} // namespace owl::app
