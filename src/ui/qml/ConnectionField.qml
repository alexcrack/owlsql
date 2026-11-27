// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.Style
import OwlSql.Theme

// One field of a driver's connection form (decision 166), rendered from its description:
// text, password, integer, bool, choice, file / directory (with Browse), multiline.
ColumnLayout {
    id: root

    required property var field // Sessions.form() entry
    property var value
    property string error
    property string passwordNote // password fields: shown as placeholder ("Stored in keychain")

    readonly property string type: field.type

    signal edited(var value)
    signal browseRequested(bool directory)

    spacing: Theme.space.xxs

    Label {
        visible: root.type !== "bool"
        text: root.field.required ? qsTr("%1 *").arg(root.field.label) : root.field.label
        font: Theme.font.uiSmall
        color: Theme.color.textSecondary
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.xs

        TextField {
            id: textInput

            Layout.fillWidth: true
            visible: ["text", "password", "integer", "file", "directory"].includes(root.type)
            text: root.value === undefined || root.value === null ? "" : String(root.value)
            placeholderText: root.type === "password" && root.passwordNote !== ""
                             ? root.passwordNote : root.field.placeholder
            echoMode: root.type === "password" ? TextInput.Password : TextInput.Normal
            inputMethodHints: root.type === "integer" ? Qt.ImhDigitsOnly : Qt.ImhNone
            validator: root.type === "integer" ? integerValidator : null
            error: root.error !== ""
            Accessible.name: root.field.label
            onTextEdited: root.edited(root.type === "integer" && textInput.text !== "" ? Number(
                                                                                             textInput.text) :
                                                                                         textInput.text)

            IntValidator {
                id: integerValidator

                bottom: root.field.minimum
                top: root.field.maximum > 0 ? root.field.maximum : 2147483647
            }
        }

        Button {
            visible: root.type === "file" || root.type === "directory"
            text: qsTr("Browse…")
            small: true
            onClicked: root.browseRequested(root.type === "directory")
        }

        ComboBox {
            id: choice

            Layout.fillWidth: true
            visible: root.type === "choice"
            model: root.field.choices
            textRole: "label"
            valueRole: "value"
            currentIndex: root.type === "choice" ? root.field.choices.findIndex(c => c.value === String(
                                                                                         root.value
                                                                                         ?? root.field.default
                                                                                         ?? "")) :
                                                   -1
            Accessible.name: root.field.label
            onActivated: root.edited(currentValue)
        }

        CheckBox {
            visible: root.type === "bool"
            text: root.field.label
            checked: root.value === true
            onToggled: root.edited(checked)
        }

        TextArea {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            visible: root.type === "multiline"
            text: root.value ?? ""
            wrapMode: TextEdit.Wrap
            Accessible.name: root.field.label
            onTextChanged: if (activeFocus)
                               root.edited(text)
        }
    }

    Label {
        Layout.fillWidth: true
        visible: root.error !== "" || root.field.help !== ""
        text: root.error !== "" ? root.error : root.field.help
        font: Theme.font.uiSmall
        color: root.error !== "" ? Theme.color.danger : Theme.color.textMuted
        wrapMode: Text.Wrap
    }
}
