// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.Style
import OwlSql.Theme

// Centered placeholder: icon, title, explanation and an optional action.
Item {
    id: root

    property string iconName
    property string title
    property string text
    property string actionText
    property bool actionEnabled: true

    signal actionTriggered

    implicitWidth: 240
    implicitHeight: column.implicitHeight

    ColumnLayout {
        id: column

        anchors.centerIn: parent
        width: Math.max(0, Math.min(360, root.width - 2 * Theme.space.xl))
        spacing: Theme.space.s

        OwlIcon {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: Theme.space.xs
            visible: root.iconName.length > 0
            name: root.iconName
            size: 28
            stroke: 1.5
            color: Theme.color.textMuted
        }

        Label {
            Layout.fillWidth: true
            text: root.title
            font: Theme.font.title
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            visible: root.text.length > 0
            text: root.text
            color: Theme.color.textSecondary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space.s
            visible: root.actionText.length > 0
            enabled: root.actionEnabled
            text: root.actionText
            onClicked: root.actionTriggered()
        }
    }
}
