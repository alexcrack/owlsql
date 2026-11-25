// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Effects
import OwlSql.Theme

// Internal: raised surface with border and soft shadow for popups, menus and dialogs.
// The shadow is a separate item behind the surface: where shader effects are unavailable
// (software renderer in VMs / remote desktops) only the shadow disappears, never the surface.
Item {
    id: root

    property alias color: surface.color
    property alias radius: surface.radius

    implicitWidth: 160
    implicitHeight: Theme.size.row

    Rectangle {
        id: shadowSource

        anchors.fill: parent
        radius: surface.radius
        color: "black"
        visible: false
    }

    MultiEffect {
        anchors.fill: shadowSource
        source: shadowSource
        shadowEnabled: true
        shadowColor: Theme.color.shadow
        shadowBlur: 0.8
        shadowVerticalOffset: 4
        autoPaddingEnabled: true
        opacity: 0.9
    }

    Rectangle {
        id: surface

        anchors.fill: parent
        radius: Theme.radius.popup
        color: Theme.color.bgRaised
        border.color: Theme.color.borderDefault
    }
}
