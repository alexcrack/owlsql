// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OwlSql.Theme

// Temporary theme preview — replaced by the shell later in M0.
ApplicationWindow {
    id: window

    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 500
    visible: true
    title: qsTr("OwlSql")
    color: Theme.color.bgCanvas

    Shortcut {
        sequence: "Ctrl+Shift+T"
        onActivated: Theme.mode = Theme.dark ? Theme.Light : Theme.Dark
    }

    Rectangle {
        anchors.centerIn: parent
        width: 560
        height: content.implicitHeight + 2 * Theme.space.xxl
        radius: Theme.radius.popup
        color: Theme.color.bgSurface
        border.color: Theme.color.borderDefault

        ColumnLayout {
            id: content

            anchors.fill: parent
            anchors.margins: Theme.space.xxl
            spacing: Theme.space.l

            Label {
                text: qsTr("OwlSql %1").arg(Qt.application.version)
                font: Theme.font.heading
                color: Theme.color.textPrimary
            }

            Label {
                text: qsTr("%1 · Ctrl+Shift+T toggles light/dark").arg(Theme.name)
                font: Theme.font.ui
                color: Theme.color.textSecondary
            }

            Row {
                spacing: Theme.space.m

                Repeater {
                    model: ["database", "table", "eye", "math-function", "key", "search", "history",
                        "settings", "player-play", "sun", "moon"]

                    delegate: OwlIcon {
                        required property string modelData
                        name: modelData
                    }
                }
            }

            Row {
                spacing: Theme.space.s

                Repeater {
                    model: [Theme.color.accentFill, Theme.color.danger, Theme.color.warning,
                        Theme.color.success, Theme.color.info, Theme.color.tagTeal,
                        Theme.color.tagViolet, Theme.color.tagPink]

                    delegate: Rectangle {
                        required property color modelData
                        width: Theme.size.control
                        height: Theme.size.control
                        radius: Theme.radius.control
                        color: modelData
                    }
                }
            }

            Label {
                text: "SELECT id, email FROM users WHERE created_at > now() - interval '7 days';"
                font: Theme.font.code
                color: Theme.color.textPrimary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Label {
                text: "NULL"
                font.family: Theme.font.grid.family
                font.pixelSize: Theme.font.grid.pixelSize
                font.italic: true
                color: Theme.color.textMuted
            }
        }
    }
}
