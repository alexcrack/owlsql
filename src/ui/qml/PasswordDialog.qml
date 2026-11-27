// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Asked when a session has no stored password (secret mode "ask", nothing in the keychain, or no
// keychain at all).
Dialog {
    id: root

    property string sessionId
    property string sessionName

    function ask(id: string, name: string) {
        sessionId = id;
        sessionName = name;
        password.text = "";
        remember.checked = false;
        open();
    }

    title: qsTr("Password for %1").arg(sessionName)
    modal: true
    width: 400
    standardButtons: Dialog.Ok | Dialog.Cancel
    onOpened: password.forceActiveFocus()
    onAccepted: Sessions.providePassword(sessionId, password.text, remember.checked)
    onRejected: Sessions.close(sessionId)

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.m

        TextField {
            id: password

            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("Password")
            Accessible.name: qsTr("Password")
            onAccepted: root.accept()
        }

        CheckBox {
            id: remember

            visible: Sessions.keychainAvailable
            text: qsTr("Remember in the system keychain")
        }

        Label {
            Layout.fillWidth: true
            visible: !Sessions.keychainAvailable
            text: qsTr("No system keychain is available, so passwords are asked on every connect.")
            font: Theme.font.uiSmall
            color: Theme.color.textMuted
            wrapMode: Text.Wrap
        }
    }
}
