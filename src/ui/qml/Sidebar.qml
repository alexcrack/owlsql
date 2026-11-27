// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Left pane (decision 010). Lists the open sessions until the object tree arrives (M1.10).
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
                text: qsTr("Open sessions")
                font: Theme.font.uiSmall
                color: Theme.color.textSecondary
            }

            ToolButton {
                icon.name: "database-plus"
                enabled: Sessions.available
                ToolTip.text: Commands.title("session.new")
                onClicked: Commands.trigger("session.new")
            }
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: count > 0
            clip: true
            model: Sessions.openSessions
            spacing: Theme.space.xxs

            delegate: ItemDelegate {
                id: row

                required property string modelData
                property var info: Sessions.info(modelData)

                width: ListView.view.width
                highlighted: modelData === Sessions.activeSession
                onClicked: Sessions.activeSession = modelData
                Accessible.name: info.name ?? ""

                Connections {
                    target: Sessions

                    function onSessionStateChanged(sessionId: string) {
                        if (sessionId === row.modelData)
                            row.info = Sessions.info(row.modelData);
                    }
                }

                contentItem: RowLayout {
                    spacing: Theme.space.s

                    TagDot {
                        tag: row.info.tag ?? "green"
                    }

                    Label {
                        Layout.fillWidth: true
                        text: row.info.name ?? ""
                        elide: Text.ElideRight
                    }

                    SafetyBadge {
                        level: row.info.level ?? "dev"
                    }

                    BusyIndicator {
                        visible: row.info.state === "connecting"
                        running: visible
                        Layout.preferredWidth: Theme.size.iconSmall
                        Layout.preferredHeight: Theme.size.iconSmall
                    }

                    ToolButton {
                        icon.name: "x"
                        padding: Theme.space.xxs
                        ToolTip.text: qsTr("Disconnect")
                        onClicked: Sessions.close(row.modelData)
                    }
                }
            }
        }

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: list.count === 0
            iconName: "database"
            title: qsTr("No open sessions")
            text: qsTr("Open a session from the Welcome tab. Its objects will appear here.")
        }
    }
}
