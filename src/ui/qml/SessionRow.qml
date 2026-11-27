// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.Style
import OwlSql.Theme

// One row of the session list: a folder (chevron, name, count) or a session (tag, name,
// engine and target, level, connection state).
ItemDelegate {
    id: root

    required property int index
    required property string kind
    required property string itemId
    required property string name
    required property string subtitle
    required property int depth
    required property bool expanded
    required property string level
    required property string tag
    required property string connectionState
    required property int childCount
    property bool selected: false

    readonly property bool isFolder: kind === "folder"

    signal folderToggled
    signal opened
    signal contextMenuRequested(point position)

    width: ListView.view ? ListView.view.width : implicitWidth
    implicitHeight: isFolder ? Theme.size.row : Theme.size.row * 1.6
    leftPadding: Theme.space.s + depth * Theme.size.treeIndent
    highlighted: selected
    Accessible.name: isFolder ? qsTr("Folder %1").arg(name) : name

    onClicked: if (isFolder)
                   folderToggled()
    onDoubleClicked: if (!isFolder)
                         opened()

    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: (point, button) => root.contextMenuRequested(point.position)
    }

    contentItem: RowLayout {
        spacing: Theme.space.s

        OwlIcon {
            visible: root.isFolder
            name: root.expanded ? "chevron-down" : "chevron-right"
            size: Theme.size.iconSmall
        }

        OwlIcon {
            visible: root.isFolder
            name: root.expanded ? "folder-open" : "folder"
            size: Theme.size.iconSmall
        }

        TagDot {
            visible: !root.isFolder
            tag: root.tag
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Label {
                Layout.fillWidth: true
                text: root.name
                elide: Text.ElideRight
                font.weight: root.connectionState === "connected" ? Font.DemiBold : Font.Normal
            }

            Label {
                Layout.fillWidth: true
                visible: !root.isFolder && root.subtitle !== ""
                text: root.subtitle
                elide: Text.ElideMiddle
                font: Theme.font.uiSmall
                color: Theme.color.textMuted
            }
        }

        Label {
            visible: root.isFolder && root.childCount > 0
            text: root.childCount
            font: Theme.font.uiSmall
            color: Theme.color.textMuted
        }

        SafetyBadge {
            visible: !root.isFolder && root.level !== "dev"
            level: root.level
        }

        BusyIndicator {
            visible: root.connectionState === "connecting"
            running: visible
            Layout.preferredWidth: Theme.size.iconSmall
            Layout.preferredHeight: Theme.size.iconSmall
        }

        Rectangle {
            visible: root.connectionState === "connected" || root.connectionState === "failed"
            Layout.preferredWidth: 6
            Layout.preferredHeight: 6
            radius: 3
            color: root.connectionState === "failed" ? Theme.color.danger : Theme.color.success
            Accessible.ignored: true
        }
    }
}
