// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Templates as T

// An Action bound to a registered command: title and shortcut come from Commands and follow
// keybinding overrides. CommandAction { command: "view.toggleSidebar" }
T.Action {
    required property string command

    text: Commands.titles[command] ?? ""
    shortcut: Commands.shortcuts[command] ?? ""
    onTriggered: Commands.trigger(command)
}
