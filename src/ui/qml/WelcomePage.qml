// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Welcome tab. The session manager moves in here in M1 (decision 065).
Rectangle {
    id: root

    color: Theme.color.bgCanvas

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(560, root.width - 2 * Theme.space.xxl)
        spacing: Theme.space.xl

        RowLayout {
            spacing: Theme.space.l

            Image {
                source: "qrc:/owlsql/logo/owlsql.svg"
                sourceSize: Qt.size(56, 56)
                Layout.preferredWidth: 56
                Layout.preferredHeight: 56
            }

            ColumnLayout {
                spacing: Theme.space.xxs

                Label {
                    text: "OwlSql"
                    font: Theme.font.heading
                }

                Label {
                    text: qsTr("A fast database client for PostgreSQL, MySQL / MariaDB and SQLite")
                    color: Theme.color.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        Frame {
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.space.m

                Label {
                    text: qsTr("Start")
                    font: Theme.font.title
                }

                RowLayout {
                    spacing: Theme.space.m

                    Button {
                        text: qsTr("New connection")
                        variant: "primary"
                        icon.name: "database-plus"
                        enabled: false
                    }

                    Button {
                        text: qsTr("Import connections")
                        enabled: false
                    }
                }

                Label {
                    text: qsTr("Connections arrive in the next milestone (M1).")
                    color: Theme.color.textMuted
                    font: Theme.font.uiSmall
                }
            }
        }

        Frame {
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.space.s

                Label {
                    text: qsTr("Keyboard")
                    font: Theme.font.title
                    Layout.bottomMargin: Theme.space.xs
                }

                Repeater {
                    model: ["view.toggleSidebar", "view.toggleBottomPanel", "view.toggleTheme",
                        "workspace.closeTab"]

                    delegate: RowLayout {
                        id: shortcutRow

                        required property string modelData

                        Layout.fillWidth: true

                        Label {
                            Layout.fillWidth: true
                            text: Commands.titles[shortcutRow.modelData] ?? ""
                            color: Theme.color.textSecondary
                        }

                        KeyChip {
                            text: Commands.shortcutTexts[shortcutRow.modelData] ?? ""
                        }
                    }
                }
            }
        }
    }
}
