// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import OwlSql.Style
import OwlSql.Theme

// "Delete X?" style confirmation with a destructive default button.
Dialog {
    id: root

    property string text
    property string confirmText: qsTr("Delete")

    signal confirmed

    modal: true
    width: 400
    onAccepted: confirmed()

    Label {
        anchors.left: parent.left
        anchors.right: parent.right
        text: root.text
        wrapMode: Text.Wrap
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: root.confirmText
            variant: "danger"
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
