// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Import sessions (decision 170): an OwlSql sessions file, ~/.pgpass or ~/.my.cnf. Shows what
// the file holds, asks what to do with sessions that already exist, then imports.
Dialog {
    id: root

    property var preview: ({})

    signal imported(int count)

    function start() {
        preview = {};
        fileDialog.open();
    }

    title: qsTr("Import connections")
    modal: true
    width: 480

    FileDialog {
        id: fileDialog

        title: qsTr("Choose a sessions file, .pgpass or .my.cnf")
        nameFilters: [qsTr("All supported (*.json *.pgpass pgpass.conf *.cnf *.ini)"), qsTr(
                "All files (*)")]
        onAccepted: {
            root.preview = Sessions.inspectImport(selectedFile);
            root.open();
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Theme.space.m

        Label {
            Layout.fillWidth: true
            visible: root.preview.ok === false
            text: root.preview.error ?? ""
            color: Theme.color.danger
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            visible: root.preview.ok === true
            text: qsTr("%n session(s) found.", "", root.preview.sessions ?? 0)
            wrapMode: Text.Wrap
        }

        Repeater {
            model: root.preview.warnings ?? []

            delegate: Label {
                required property string modelData

                Layout.fillWidth: true
                text: "• " + modelData
                font: Theme.font.uiSmall
                color: Theme.color.warning
                wrapMode: Text.Wrap
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: (root.preview.conflicts ?? 0) > 0
            spacing: Theme.space.xs

            Label {
                Layout.fillWidth: true
                text: qsTr("%n session(s) already exist here:", "", root.preview.conflicts ?? 0)
                wrapMode: Text.Wrap
            }

            RadioButton {
                id: keepBoth

                checked: true
                text: qsTr("Keep both")
            }

            RadioButton {
                id: replace

                text: qsTr("Replace mine")
            }

            RadioButton {
                text: qsTr("Skip them")
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: qsTr("Import")
            variant: "primary"
            enabled: root.preview.ok === true && (root.preview.sessions ?? 0) > 0
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        const r = Sessions.applyImport(keepBoth.checked ? "keepBoth" : replace.checked ? "replace" :
                                                                                         "skip");
        if (r.ok)
            imported(r.imported);
    }
}
