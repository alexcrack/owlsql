// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import OwlSql.Theme

// Internal: 2px accent ring drawn just outside its parent, shown for keyboard focus only.
Rectangle {
    property real baseRadius: Theme.radius.control

    anchors.fill: parent
    anchors.margins: -3
    radius: baseRadius + 3
    color: "transparent"
    border.width: 2
    border.color: Theme.color.accentLine
}
