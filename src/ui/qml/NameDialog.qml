// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import OwlSql.Style
import OwlSql.Theme

// Asks for a name (new folder, rename).
Dialog {
    id: root

    property alias name: field.text
    property string placeholder

    signal named(string name)

    modal: true
    width: 360
    standardButtons: Dialog.Ok | Dialog.Cancel
    onOpened: {
        field.forceActiveFocus();
        field.selectAll();
    }
    onAccepted: if (field.text.trim() !== "")
                    named(field.text.trim())

    TextField {
        id: field

        anchors.left: parent.left
        anchors.right: parent.right
        placeholderText: root.placeholder
        Accessible.name: root.title
        onAccepted: root.accept()
    }
}
