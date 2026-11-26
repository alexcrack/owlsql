// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls

// Plain TextArea in a ScrollView, the way SqlEditor.qml would host it (specs/sql-editor.md §1).
ApplicationWindow {
    id: window

    width: 1200
    height: 800
    visible: true
    title: "M1.2 editor spike"

    ScrollView {
        objectName: "scroll"
        anchors.fill: parent

        TextArea {
            objectName: "area"
            font.family: "JetBrains Mono, monospace"
            font.pixelSize: 13
            wrapMode: TextEdit.NoWrap
            textFormat: TextEdit.PlainText
            selectByMouse: true
            persistentSelection: true
        }
    }
}
