// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.Style
import OwlSql.Theme

// Left pane: object tree of all connections (decision 010). Placeholder until M1.
Rectangle {
    id: root

    color: Theme.color.bgSurface

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.m
        spacing: Theme.space.m

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xs

            Label {
                Layout.fillWidth: true
                leftPadding: Theme.space.xs
                text: qsTr("Connections")
                font: Theme.font.uiSmall
                color: Theme.color.textSecondary
            }

            ToolButton {
                icon.name: "database-plus"
                enabled: false
                ToolTip.text: qsTr("New connection")
            }
        }

        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Filter objects")
            enabled: false
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconName: "database"
            title: qsTr("No connections yet")
            text: qsTr("Saved sessions and their objects will appear here.")
        }
    }
}
