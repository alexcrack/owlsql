// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import OwlSql.Theme

// Displays a keyboard shortcut, e.g. KeyChip { text: Commands.shortcutText("view.toggleSidebar") }
Rectangle {
    id: root

    property alias text: label.text

    implicitWidth: label.implicitWidth + 2 * Theme.space.s
    implicitHeight: label.implicitHeight + Theme.space.xxs * 2
    visible: label.text.length > 0
    radius: Theme.radius.control
    color: Theme.color.bgSunken
    border.color: Theme.color.borderDefault

    Text {
        id: label

        anchors.centerIn: parent
        font: Theme.font.uiTiny
        color: Theme.color.textSecondary
    }
}
