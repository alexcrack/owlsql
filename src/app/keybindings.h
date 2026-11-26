// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

namespace owl::app {

class CommandRegistry;
class WatchedFile;

// keybindings.json: shortcut overrides applied to the command registry, reloaded live.
//   [ { "command": "view.toggleBottomPanel", "key": "Ctrl+Alt+J" },
//     { "command": "view.toggleTheme", "key": "" } ]          <- "" removes the shortcut
// Keys use portable text ("Ctrl" = Cmd on macOS).
class Keybindings : public QObject
{
    Q_OBJECT

public:
    Keybindings(const QString &filePath, CommandRegistry *registry, QObject *parent);

    void reload();
    QStringList problems() const { return m_problems; }

signals:
    void problemsChanged();

private:
    CommandRegistry *m_registry;
    WatchedFile *m_file;
    QStringList m_problems;
};

} // namespace owl::app
