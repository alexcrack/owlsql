// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Log / History / Messages (decision 068). Collapsed by default; Ctrl+J toggles.
Rectangle {
    id: root

    property alias currentIndex: tabs.currentIndex

    color: Theme.color.bgSurface

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space.s
            Layout.rightMargin: Theme.space.xs
            spacing: 0

            TabBar {
                id: tabs

                Layout.fillWidth: true

                TabButton {
                    text: qsTr("Log")
                    width: implicitWidth
                }
                TabButton {
                    text: qsTr("History")
                    width: implicitWidth
                }
                TabButton {
                    text: qsTr("Messages")
                    width: implicitWidth
                }
            }

            ToolButton {
                icon.name: "x"
                ToolTip.text: Commands.title("view.toggleBottomPanel")
                onClicked: Commands.trigger("view.toggleBottomPanel")
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            EmptyState {
                iconName: "terminal-2"
                title: qsTr("No statements yet")
                text: qsTr("Every statement OwlSql sends to a server is logged here.")
            }
            EmptyState {
                iconName: "history"
                title: qsTr("No history yet")
                text: qsTr("Queries you run are kept here and can be searched.")
            }
            EmptyState {
                iconName: "message-2"
                title: qsTr("No messages")
                text: qsTr("Server notices, warnings and errors appear here.")
            }
        }
    }
}
