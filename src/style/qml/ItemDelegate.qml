// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.ItemDelegate {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0
    horizontalPadding: Theme.space.m
    spacing: Theme.space.s
    font: Theme.font.ui
    hoverEnabled: true

    contentItem: ButtonContent {
        text: control.text
        iconName: control.icon.name
        display: control.display
        alignment: Qt.AlignLeft
        font: control.font
        spacing: control.spacing
        color: control.enabled ? Theme.color.textPrimary : Theme.color.textDisabled
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: Theme.size.row
        radius: Theme.radius.control
        color: control.highlighted ? Theme.color.bgSelected : control.down
                                     ? Theme.color.bgSelectedInactive : control.hovered
                                       ? Theme.color.bgHover : "transparent"
    }
}
