// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Welcome tab = the session manager (decision 065, design-system.md §8.2): saved sessions on the
// left, the selected (or new) session's settings on the right.
Rectangle {
    id: root

    property string selectedId
    property bool editing: false // the editor shows a session or a new draft

    function select(sessionId: string) {
        if (sessionId === "")
            return;
        selectedId = sessionId;
        editor.load(Sessions.draft(sessionId));
        editing = true;
    }

    function startNew(folderId: string) {
        selectedId = "";
        const drivers = Sessions.drivers;
        editor.load(Sessions.newDraft(drivers.length > 0 ? drivers[0].id : "", folderId));
        editing = true;
    }

    function startImport() {
        importDialog.start();
    }

    color: Theme.color.bgCanvas

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.xl
        spacing: Theme.space.l

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.m

            Image {
                source: "qrc:/owlsql/logo/owlsql.svg"
                sourceSize: Qt.size(40, 40)
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                Accessible.ignored: true
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                Label {
                    text: "OwlSql"
                    font: Theme.font.heading
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Connections to PostgreSQL, MySQL / MariaDB and SQLite")
                    color: Theme.color.textSecondary
                    elide: Text.ElideRight
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !Sessions.available
            text: qsTr("Sessions cannot be saved: OwlSql's own database could not be opened.")
            color: Theme.color.danger
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space.l

            SessionList {
                Layout.preferredWidth: 340
                Layout.minimumWidth: 260
                Layout.fillHeight: true
                selectedId: root.selectedId
                onSessionSelected: sessionId => root.select(sessionId)
                onNewRequested: folderId => root.startNew(folderId)
                onImportRequested: root.startImport()
                onExportRequested: exportDialog.open()
                onFolderNameRequested: (folderId, parentId, currentName) => {
                    nameDialog.folderId = folderId;
                    nameDialog.parentId = parentId;
                    nameDialog.title = folderId === "" ? qsTr("New folder") : qsTr("Rename folder");
                    nameDialog.name = currentName;
                    nameDialog.open();
                }
                onRemoveRequested: (sessionId, name) => {
                    confirmDialog.action = () => {
                        Sessions.remove(sessionId);
                        if (root.selectedId === sessionId) {
                            root.selectedId = "";
                            root.editing = false;
                        }
                    };
                    confirmDialog.title = qsTr("Delete session");
                    confirmDialog.text = qsTr("Delete \"%1\" and its stored password?").arg(name);
                    confirmDialog.open();
                }
                onRemoveFolderRequested: (folderId, name) => {
                    confirmDialog.action = () => Sessions.removeFolder(folderId);
                    confirmDialog.title = qsTr("Delete folder");
                    confirmDialog.text = qsTr(
                                "Delete the folder \"%1\" and its subfolders? Its sessions move to the top level.").arg(
                                name);
                    confirmDialog.open();
                }
            }

            SessionEditor {
                id: editor

                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.editing
                onSaved: sessionId => root.selectedId = sessionId
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !root.editing

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 420)
                    spacing: Theme.space.m

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Start")
                        font: Theme.font.title
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr(
                                  "Choose a session on the left, or create one. Passwords go to your system keychain, never to OwlSql's files.")
                        color: Theme.color.textSecondary
                        wrapMode: Text.Wrap
                    }

                    RowLayout {
                        spacing: Theme.space.s

                        Button {
                            text: Commands.title("session.new")
                            icon.name: "database-plus"
                            variant: "primary"
                            enabled: Sessions.available
                            onClicked: root.startNew("")
                        }

                        Button {
                            text: Commands.title("session.import")
                            icon.name: "file-import"
                            enabled: Sessions.available
                            onClicked: root.startImport()
                        }
                    }

                    RowLayout {
                        Layout.topMargin: Theme.space.l
                        spacing: Theme.space.s

                        Label {
                            Layout.fillWidth: true
                            text: Commands.title("session.new")
                            color: Theme.color.textSecondary
                        }

                        KeyChip {
                            text: Commands.shortcutTexts["session.new"] ?? ""
                        }
                    }
                }
            }
        }
    }

    NameDialog {
        id: nameDialog

        property string folderId
        property string parentId

        placeholder: qsTr("Folder name")
        onNamed: name => {
            if (folderId === "")
                Sessions.addFolder(name, parentId);
            else
                Sessions.renameFolder(folderId, name);
        }
    }

    ConfirmDialog {
        id: confirmDialog

        property var action

        onConfirmed: action()
    }

    ImportDialog {
        id: importDialog
    }

    FileDialog {
        id: exportDialog

        title: qsTr("Export sessions")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("OwlSql sessions (*.owlsql-sessions.json)")]
        onAccepted: Sessions.exportTo(selectedFile)
    }
}
