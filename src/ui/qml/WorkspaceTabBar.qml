// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import OwlSql.Style
import OwlSql.Theme

// Underline-style workspace tabs (decision 061). The model provides `title` and `icon` roles.
Rectangle {
    id: root

    property alias model: list.model
    property int currentIndex: -1

    signal activated(int index)
    signal closeRequested(int index)

    implicitHeight: Theme.size.tabBar
    color: Theme.color.bgSurface

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.color.borderDefault
    }

    ListView {
        id: list

        anchors.fill: parent
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        currentIndex: root.currentIndex

        delegate: Item {
            id: tab

            required property int index
            required property string title
            required property string icon

            readonly property bool current: index === root.currentIndex

            width: row.implicitWidth + 2 * Theme.space.m
            height: ListView.view.height

            Rectangle {
                anchors.fill: parent
                color: tabHover.hovered && !tab.current ? Theme.color.bgHover : "transparent"
            }

            Row {
                id: row

                anchors.centerIn: parent
                spacing: Theme.space.s

                OwlIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: tab.icon
                    color: tab.current ? Theme.color.textPrimary : Theme.color.textSecondary
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: tab.title
                    font: Theme.font.ui
                    color: tab.current ? Theme.color.textPrimary : Theme.color.textSecondary
                }

                ToolButton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.size.controlSmall
                    height: Theme.size.controlSmall
                    padding: Theme.space.xxs
                    icon.name: "x"
                    opacity: tab.current || tabHover.hovered ? 1 : 0
                    onClicked: root.closeRequested(tab.index)
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 2
                color: Theme.color.accentLine
                visible: tab.current
            }

            HoverHandler {
                id: tabHover
            }

            TapHandler {
                acceptedButtons: Qt.LeftButton
                onTapped: root.activated(tab.index)
            }

            TapHandler {
                acceptedButtons: Qt.MiddleButton
                onTapped: root.closeRequested(tab.index)
            }
        }
    }
}
