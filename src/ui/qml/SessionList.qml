// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Left half of the session manager (design-system.md §8.2): search, folders and sessions,
// New / Import / Export.
Frame {
    id: root

    property string selectedId

    signal sessionSelected(string sessionId)
    signal newRequested(string folderId)
    signal importRequested
    signal exportRequested
    signal folderNameRequested(string folderId, string parentId, string currentName)
    signal removeRequested(string sessionId, string name)
    signal removeFolderRequested(string folderId, string name)

    padding: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TextField {
            id: search

            Layout.fillWidth: true
            Layout.margins: Theme.space.s
            placeholderText: qsTr("Search sessions")
            onTextChanged: Sessions.model.filter = text
            Keys.onDownPressed: list.forceActiveFocus()
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Sessions.model
            activeFocusOnTab: true
            currentIndex: Sessions.model.rowOf(root.selectedId)
            ScrollBar.vertical: ScrollBar {}
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Sessions")

            Keys.onReturnPressed: {
                if (currentItem && root.selectedId !== "")
                    Sessions.open(root.selectedId);
            }
            Keys.onUpPressed: list.select(Math.max(0, list.currentIndex - 1))
            Keys.onDownPressed: list.select(Math.min(list.count - 1, list.currentIndex + 1))

            function select(row: int) {
                const item = list.itemAtIndex(row) as SessionRow;
                if (!item)
                    return;
                if (item.isFolder)
                    list.currentIndex = row;
                else
                    root.sessionSelected(item.itemId);
            }

            delegate: SessionRow {
                id: row

                selected: !isFolder && itemId === root.selectedId
                onClicked: {
                    list.forceActiveFocus();
                    if (!isFolder)
                        root.sessionSelected(itemId);
                }
                onFolderToggled: Sessions.setFolderExpanded(itemId, !expanded)
                onOpened: Sessions.open(itemId)
                onContextMenuRequested: position => {
                    contextMenu.row = row;
                    contextMenu.popup(row, position);
                }
            }

            EmptyState {
                anchors.fill: parent
                visible: list.count === 0
                iconName: Sessions.model.filter === "" ? "database" : "search"
                title: Sessions.model.filter === "" ? qsTr("No sessions yet") : qsTr(
                                                          "No matching sessions")
                text: Sessions.model.filter === "" ? qsTr(
                                                         "Create a connection or import your existing ones.") :
                                                     ""
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.color.borderSubtle
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space.s
            spacing: Theme.space.xs

            Button {
                text: qsTr("New")
                icon.name: "plus"
                small: true
                variant: "primary"
                onClicked: root.newRequested("")
            }

            Button {
                text: qsTr("Folder")
                icon.name: "folder-plus"
                small: true
                onClicked: root.folderNameRequested("", "", "")
            }

            Item {
                Layout.fillWidth: true
            }

            ToolButton {
                icon.name: "file-import"
                ToolTip.text: Commands.title("session.import")
                onClicked: root.importRequested()
            }

            ToolButton {
                icon.name: "file-export"
                enabled: Sessions.model.sessionCount > 0
                ToolTip.text: qsTr("Export sessions…")
                onClicked: root.exportRequested()
            }
        }
    }

    Menu {
        id: contextMenu

        property SessionRow row

        readonly property bool onFolder: row !== null && row.isFolder
        readonly property bool isOpen: row !== null && row.connectionState !== ""

        MenuItem {
            visible: !contextMenu.onFolder
            height: visible ? implicitHeight : 0
            text: contextMenu.isOpen ? qsTr("Disconnect") : qsTr("Connect")
            onTriggered: contextMenu.isOpen ? Sessions.close(contextMenu.row.itemId) : Sessions.open(
                                                  contextMenu.row.itemId)
        }
        MenuItem {
            visible: !contextMenu.onFolder
            height: visible ? implicitHeight : 0
            text: qsTr("Duplicate")
            onTriggered: root.sessionSelected(Sessions.duplicate(contextMenu.row.itemId))
        }
        MenuItem {
            visible: contextMenu.onFolder
            height: visible ? implicitHeight : 0
            text: qsTr("New Session Here")
            onTriggered: root.newRequested(contextMenu.row.itemId)
        }
        MenuItem {
            visible: contextMenu.onFolder
            height: visible ? implicitHeight : 0
            text: qsTr("New Subfolder")
            onTriggered: root.folderNameRequested("", contextMenu.row.itemId, "")
        }
        MenuItem {
            visible: contextMenu.onFolder
            height: visible ? implicitHeight : 0
            text: qsTr("Rename…")
            onTriggered: root.folderNameRequested(contextMenu.row.itemId, "", contextMenu.row.name)
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete…")
            onTriggered: contextMenu.onFolder ? root.removeFolderRequested(contextMenu.row.itemId,
                                                                           contextMenu.row.name) :
                                                root.removeRequested(contextMenu.row.itemId,
                                                                     contextMenu.row.name)
        }
    }
}
