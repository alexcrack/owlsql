// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import OwlSql.Style
import OwlSql.Theme

// DEV / STAGE / PROD chip (decision 063). Dev sessions show nothing unless `showDev`.
Rectangle {
    id: root

    property string level: "dev"
    property bool showDev: false

    readonly property color _color: level === "prod" ? Theme.color.danger : level === "staging"
                                                       ? Theme.color.tagBlue : Theme.color.tagGreen

    visible: level !== "dev" || showDev
    implicitWidth: label.implicitWidth + 2 * Theme.space.xs
    implicitHeight: label.implicitHeight + 2
    radius: Theme.radius.control
    color: level === "prod" ? Theme.color.danger : "transparent"
    border.width: level === "prod" ? 0 : 1
    border.color: _color
    Accessible.role: Accessible.StaticText
    Accessible.name: label.text

    Label {
        id: label

        anchors.centerIn: parent
        text: root.level === "prod" ? qsTr("PROD") : root.level === "staging" ? qsTr("STAGE") : qsTr(
                                                                                    "DEV")
        font: Theme.font.uiTiny
        color: root.level === "prod" ? Theme.color.accentOnFill : root._color
    }
}
