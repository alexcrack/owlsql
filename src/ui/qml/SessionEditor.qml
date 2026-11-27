// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Right half of the session manager (design-system.md §8.2): edits one session draft. The
// connection fields are generated from the driver's form (decision 166).
Frame {
    id: root

    property var draft: ({})
    property var values: ({})
    property var errors: ({})
    property var fields: []
    property bool dirty: false
    property string message
    property bool messageIsError: false
    property string sessionState

    readonly property bool isNew: (draft.id ?? "") === ""
    readonly property bool hasPasswordField: fields.some(f => f.type === "password")
    readonly property var tabs: [
        {
            title: qsTr("Connection"),
            groups: ["connection", "authentication"]
        },
        {
            title: qsTr("TLS / SSL"),
            groups: ["ssl"]
        },
        {
            title: qsTr("Advanced"),
            groups: ["advanced"]
        }
    ].filter(t => t.groups.includes("advanced") || fields.some(f => t.groups.includes(f.group)))

    signal saved(string sessionId)

    function load(d: var) {
        draft = d;
        values = d.values ?? {};
        fields = Sessions.form(d.driverId ?? "");
        errors = {};
        dirty = false;
        message = "";
        sessionState = Sessions.state(d.id ?? "");
        tabBar.currentIndex = 0;
    }

    function setValue(key: string, value: var) {
        const v = Object.assign({}, values);
        v[key] = value;
        values = v;
        dirty = true;
        if (errors[key] !== undefined) {
            const e = Object.assign({}, errors);
            delete e[key];
            errors = e;
        }
    }

    function setDraft(key: string, value: var) {
        const d = Object.assign({}, draft);
        d[key] = value;
        draft = d;
        dirty = true;
        if (key === "name" && errors.name !== undefined) {
            const e = Object.assign({}, errors);
            delete e.name;
            errors = e;
        }
    }

    function current(): var {
        const d = Object.assign({}, draft);
        d.values = values;
        return d;
    }

    function isShown(field: var): bool {
        if (field.visibleWhenKey === "")
            return true;
        const v = values[field.visibleWhenKey] ?? "";
        return String(v) === String(field.visibleWhenValue);
    }

    function showMessage(text: string, isError: bool) {
        message = text;
        messageIsError = isError;
    }

    // Saves and returns the session id, or "" when something is wrong (shown in the form).
    function save(): string {
        errors = Sessions.validate(current());
        if (Object.keys(errors).length > 0) {
            showMessage(qsTr("Check the highlighted fields"), true);
            return "";
        }
        const r = Sessions.save(current());
        if (!r.ok) {
            showMessage(r.error, true);
            return "";
        }
        load(Sessions.draft(r.id));
        showMessage(qsTr("Saved"), false);
        saved(r.id);
        return r.id;
    }

    function openSession() {
        const id = dirty || isNew ? save() : draft.id;
        if (id !== "")
            Sessions.open(id);
    }

    function applyUrl(url: string) {
        const d = Sessions.draftFromUrl(url, draft.folderId ?? "");
        if (d.error !== undefined) {
            showMessage(d.error, true);
            return;
        }
        if (!isNew) {
            // Keep the session's identity; take the connection settings from the URL.
            for (const key of ["id", "name", "folderId", "level", "tag", "notes", "startupScript",
                               "readOnly"])
                d[key] = draft[key];
        }
        load(d);
        dirty = true;
        showMessage(qsTr("Settings taken from the URL"), false);
    }

    padding: Theme.space.l

    Connections {
        target: Sessions

        function onSessionStateChanged(sessionId: string) {
            if (sessionId === (root.draft.id ?? ""))
                root.sessionState = Sessions.state(sessionId);
        }

        function onTestFinished(ok: bool, message: string) {
            root.showMessage(message, !ok);
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.m

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.s

            TagDot {
                tag: root.draft.tag || (root.draft.level === "prod" ? "red" : root.draft.level
                                                                      === "staging" ? "blue" :
                                                                                      "green")
                diameter: 10
            }

            Label {
                Layout.fillWidth: true
                text: (root.draft.name ?? "") !== "" ? root.draft.name : qsTr("New connection")
                font: Theme.font.title
                elide: Text.ElideRight
            }

            Label {
                visible: root.sessionState !== ""
                text: root.sessionState === "connected" ? qsTr("Connected") : root.sessionState
                                                          === "connecting" ? qsTr("Connecting…") :
                                                                             root.sessionState
                                                                             === "failed" ? qsTr(
                                                                                                "Failed") :
                                                                                            qsTr("Password needed")
                font: Theme.font.uiSmall
                color: root.sessionState === "failed" ? Theme.color.danger : root.sessionState
                                                        === "connected" ? Theme.color.success :
                                                                          Theme.color.textSecondary
            }

            SafetyBadge {
                level: root.draft.level ?? "dev"
                showDev: true
            }
        }

        ScrollView {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Theme.space.m

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.space.m
                    rowSpacing: Theme.space.m

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.columnSpan: 2
                        visible: root.isNew
                        spacing: Theme.space.xxs

                        Label {
                            text: qsTr("Engine")
                            font: Theme.font.uiSmall
                            color: Theme.color.textSecondary
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: Sessions.drivers
                            textRole: "name"
                            valueRole: "id"
                            currentIndex: Sessions.drivers.findIndex(d => d.id
                                                                          === root.draft.driverId)
                            Accessible.name: qsTr("Engine")
                            onActivated: {
                                const d = Sessions.newDraft(currentValue, root.draft.folderId
                                                            ?? "");
                                d.name = root.draft.name ?? "";
                                d.level = root.draft.level ?? "dev";
                                d.tag = root.draft.tag ?? "";
                                root.load(d);
                                root.dirty = true;
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.xxs

                        Label {
                            text: qsTr("Name *")
                            font: Theme.font.uiSmall
                            color: Theme.color.textSecondary
                        }

                        TextField {
                            id: nameField

                            Layout.fillWidth: true
                            text: root.draft.name ?? ""
                            placeholderText: qsTr("e.g. prod-pg")
                            error: root.errors.name !== undefined
                            Accessible.name: qsTr("Name")
                            onTextEdited: root.setDraft("name", text)
                        }

                        Label {
                            visible: root.errors.name !== undefined
                            text: root.errors.name ?? ""
                            font: Theme.font.uiSmall
                            color: Theme.color.danger
                        }
                    }

                    ColumnLayout {
                        id: folderColumn

                        readonly property var folders: [
                            {
                                "id": "",
                                "name": qsTr("(none)")
                            }
                        ].concat(Sessions.folders.map(f => ({
                            "id": f.id,
                            "name": "    ".repeat(f.depth) + f.name
                        })))

                        Layout.fillWidth: true
                        spacing: Theme.space.xxs

                        Label {
                            text: qsTr("Folder")
                            font: Theme.font.uiSmall
                            color: Theme.color.textSecondary
                        }

                        ComboBox {
                            Layout.fillWidth: true
                            model: folderColumn.folders
                            textRole: "name"
                            valueRole: "id"
                            currentIndex: folderColumn.folders.findIndex(f => f.id === (
                                                                                  root.draft.folderId
                                                                                  ?? ""))
                            Accessible.name: qsTr("Folder")
                            onActivated: root.setDraft("folderId", currentValue)
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.xxs

                        Label {
                            text: qsTr("Safety level")
                            font: Theme.font.uiSmall
                            color: Theme.color.textSecondary
                        }

                        RowLayout {
                            spacing: Theme.space.xxs

                            Repeater {
                                model: [
                                    {
                                        "level": "dev",
                                        "title": qsTr("Dev")
                                    },
                                    {
                                        "level": "staging",
                                        "title": qsTr("Staging")
                                    },
                                    {
                                        "level": "prod",
                                        "title": qsTr("Prod")
                                    }
                                ]

                                delegate: Button {
                                    required property var modelData

                                    text: modelData.title
                                    small: true
                                    checkable: true
                                    checked: (root.draft.level ?? "dev") === modelData.level
                                    variant: checked ? "primary" : "secondary"
                                    onClicked: root.setDraft("level", modelData.level)
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.xxs

                        Label {
                            text: qsTr("Colour")
                            font: Theme.font.uiSmall
                            color: Theme.color.textSecondary
                        }

                        RowLayout {
                            spacing: Theme.space.xs

                            Repeater {
                                model: ["", "red", "orange", "amber", "green", "teal", "blue", "violet",
                                    "pink"]

                                delegate: TagSwatch {
                                    required property string modelData

                                    tag: modelData
                                    selected: (root.draft.tag ?? "") === modelData
                                    onClicked: root.setDraft("tag", modelData)
                                }
                            }
                        }
                    }
                }

                TabBar {
                    id: tabBar

                    Layout.fillWidth: true

                    Repeater {
                        model: root.tabs

                        delegate: TabButton {
                            required property var modelData

                            text: modelData.title
                        }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    currentIndex: tabBar.currentIndex

                    Repeater {
                        model: root.tabs

                        delegate: ColumnLayout {
                            id: tab

                            required property var modelData

                            spacing: Theme.space.m

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: Theme.space.m
                                rowSpacing: Theme.space.m

                                Repeater {
                                    model: root.fields.filter(f => tab.modelData.groups.includes(
                                                                       f.group))

                                    delegate: ConnectionField {
                                        required property var modelData

                                        field: modelData
                                        visible: root.isShown(modelData)
                                        Layout.fillWidth: true
                                        Layout.columnSpan: ["file", "directory", "multiline",
                                            "bool"].includes(modelData.type) ? 2 : 1
                                        value: root.values[modelData.key]
                                        error: root.errors[modelData.key] ?? ""
                                        passwordNote: root.draft.hasStoredPassword ? qsTr(
                                                                                         "Stored in the keychain") :
                                                                                     ""
                                        onEdited: value => root.setValue(modelData.key, value)
                                        onBrowseRequested: directory => {
                                            if (directory) {
                                                folderDialog.key = modelData.key;
                                                folderDialog.open();
                                            } else {
                                                fileDialog.key = modelData.key;
                                                fileDialog.open();
                                            }
                                        }
                                    }
                                }
                            }

                            CheckBox {
                                visible: root.hasPasswordField && tab.modelData.groups.includes(
                                             "authentication")
                                text: Sessions.keychainAvailable ? qsTr(
                                                                       "Save the password in the system keychain") :
                                                                   qsTr("No keychain available — the password is asked on connect")
                                enabled: Sessions.keychainAvailable
                                checked: root.draft.savePassword ?? false
                                onToggled: root.setDraft("savePassword", checked)
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: tab.modelData.groups.includes("advanced")
                                spacing: Theme.space.m

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space.xxs

                                    Label {
                                        text: qsTr("Startup script")
                                        font: Theme.font.uiSmall
                                        color: Theme.color.textSecondary
                                    }

                                    TextArea {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 72
                                        text: root.draft.startupScript ?? ""
                                        font: Theme.font.code
                                        placeholderText: qsTr(
                                                             "Runs on every new connection, e.g. SET search_path TO app")
                                        wrapMode: TextEdit.Wrap
                                        Accessible.name: qsTr("Startup script")
                                        onTextChanged: if (activeFocus)
                                                           root.setDraft("startupScript", text)
                                    }
                                }

                                CheckBox {
                                    text: qsTr("Read-only: refuse statements that write")
                                    checked: root.draft.readOnly ?? false
                                    onToggled: root.setDraft("readOnly", checked)
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space.xxs

                                    Label {
                                        text: qsTr("Notes")
                                        font: Theme.font.uiSmall
                                        color: Theme.color.textSecondary
                                    }

                                    TextArea {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 56
                                        text: root.draft.notes ?? ""
                                        wrapMode: TextEdit.Wrap
                                        Accessible.name: qsTr("Notes")
                                        onTextChanged: if (activeFocus)
                                                           root.setDraft("notes", text)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.message !== ""
            text: root.message
            color: root.messageIsError ? Theme.color.danger : Theme.color.success
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.s

            Button {
                text: qsTr("Paste URL…")
                icon.name: "link-plus"
                small: true
                onClicked: urlDialog.open()
            }

            Button {
                text: Sessions.testing ? qsTr("Stop test") : qsTr("Test")
                small: true
                onClicked: {
                    if (Sessions.testing) {
                        Sessions.cancelTest();
                        root.showMessage("", false);
                    } else {
                        root.showMessage(qsTr("Connecting…"), false);
                        Sessions.test(root.current());
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Save")
                small: true
                enabled: root.dirty || root.isNew
                onClicked: root.save()
            }

            Button {
                text: root.sessionState === "connected" ? qsTr("Disconnect") : qsTr("Open")
                icon.name: root.sessionState === "connected" ? "plug-connected-x" : "player-play"
                small: true
                variant: "primary"
                onClicked: root.sessionState === "connected" ? Sessions.close(root.draft.id) : root.openSession(
                                                                   )
            }
        }
    }

    FileDialog {
        id: fileDialog

        property string key

        title: qsTr("Choose a file")
        onAccepted: root.setValue(key, Sessions.localPath(selectedFile))
    }

    FolderDialog {
        id: folderDialog

        property string key

        title: qsTr("Choose a folder")
        onAccepted: root.setValue(key, Sessions.localPath(selectedFolder))
    }

    Dialog {
        id: urlDialog

        title: qsTr("Paste a connection URL")
        modal: true
        width: 480
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAboutToShow: urlField.text = ""
        onOpened: urlField.forceActiveFocus()
        onAccepted: root.applyUrl(urlField.text)

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.space.s

            TextField {
                id: urlField

                Layout.fillWidth: true
                placeholderText: "postgresql://user@host:5432/db"
                Accessible.name: qsTr("Connection URL")
                onAccepted: urlDialog.accept()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr(
                          "postgresql://, mysql://, mariadb://, libpq \"host=… dbname=…\" strings or an SQLite file path")
                font: Theme.font.uiSmall
                color: Theme.color.textMuted
                wrapMode: Text.Wrap
            }
        }
    }
}
