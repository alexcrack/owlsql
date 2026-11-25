// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// Placeholder window — replaced by the themed shell later in M0.
ApplicationWindow {
    id: window

    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 500
    visible: true
    title: qsTr("OwlSql")

    Label {
        anchors.centerIn: parent
        text: qsTr("OwlSql %1").arg(Qt.application.version)
    }
}
