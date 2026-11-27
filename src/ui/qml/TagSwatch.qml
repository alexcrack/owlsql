// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Templates as T
import OwlSql.Style
import OwlSql.Theme

// A pickable colour tag; tag "" means "the safety level's colour" (shown as a ring).
T.AbstractButton {
    id: root

    property string tag
    property bool selected: false

    implicitWidth: 20
    implicitHeight: 20
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.RadioButton
    Accessible.name: tag === "" ? qsTr("Level colour") : tag
    Accessible.checked: selected
    ToolTip.visible: hovered
    ToolTip.text: tag === "" ? qsTr("Use the level's colour") : tag

    background: Rectangle {
        radius: width / 2
        color: "transparent"
        border.width: root.selected || root.visualFocus ? 2 : 0
        border.color: root.visualFocus ? Theme.color.accentLine : Theme.color.textPrimary
    }

    contentItem: Item {
        TagDot {
            anchors.centerIn: parent
            visible: root.tag !== ""
            tag: root.tag
            diameter: 12
        }

        Rectangle {
            anchors.centerIn: parent
            visible: root.tag === ""
            width: 12
            height: 12
            radius: 6
            color: "transparent"
            border.width: 1
            border.color: Theme.color.textMuted
        }
    }
}
