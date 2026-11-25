// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Session · transaction · metrics · cursor (decision 067). Only the session slot is live in M0.
Rectangle {
    id: root

    implicitHeight: Theme.size.statusBar
    color: Theme.color.bgSunken

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.color.borderDefault
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.m
        anchors.rightMargin: Theme.space.xs
        spacing: Theme.space.l

        Row {
            spacing: Theme.space.s

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 8
                height: 8
                radius: 4
                color: Theme.color.textMuted
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Not connected")
                font: Theme.font.uiSmall
                color: Theme.color.textSecondary
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Label {
            text: "OwlSql " + Qt.application.version
            font: Theme.font.uiSmall
            color: Theme.color.textMuted
        }

        ToolButton {
            Layout.preferredHeight: Theme.size.statusBar - 2
            Layout.preferredWidth: Theme.size.statusBar - 2
            padding: Theme.space.xxs
            icon.name: Theme.dark ? "sun" : "moon"
            ToolTip.text: Commands.title("view.toggleTheme")
            onClicked: Commands.trigger("view.toggleTheme")
        }
    }
}
