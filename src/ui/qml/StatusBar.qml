// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Session · transaction · metrics · cursor (decision 067). The session slot shows the active
// session (M1.9b); the other slots follow with the SQL editor.
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

        RowLayout {
            id: sessionSlot

            readonly property var info: Sessions.activeInfo
            readonly property bool active: info.id !== undefined

            spacing: Theme.space.s

            SafetyBadge {
                visible: sessionSlot.active && sessionSlot.info.level === "prod"
                level: "prod"
            }

            TagDot {
                tag: sessionSlot.active ? sessionSlot.info.tag : ""
                color: sessionSlot.active ? tagColor(sessionSlot.info.tag) : Theme.color.textMuted
            }

            Label {
                Layout.maximumWidth: 420
                elide: Text.ElideRight
                font: Theme.font.uiSmall
                color: sessionSlot.info.state === "failed" ? Theme.color.danger :
                                                             Theme.color.textSecondary
                text: {
                    const i = sessionSlot.info;
                    const name = i.name ?? "";
                    if (!sessionSlot.active)
                        return qsTr("Not connected");
                    switch (i.state ?? "") {
                    case "connected":
                        return qsTr("%1 · %2 %3").arg(name).arg(i.product ?? "").arg(i.version
                                                                                     ?? "");
                    case "connecting":
                        return qsTr("%1 · connecting…").arg(name);
                    case "needsPassword":
                        return qsTr("%1 · waiting for the password").arg(name);
                    case "failed":
                        return qsTr("%1 · %2").arg(name).arg(i.error ?? "");
                    default:
                        return name;
                    }
                }
                ToolTip.visible: hoverHandler.hovered && sessionSlot.info.state === "failed"
                ToolTip.text: sessionSlot.info.error ?? ""

                HoverHandler {
                    id: hoverHandler
                }
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
