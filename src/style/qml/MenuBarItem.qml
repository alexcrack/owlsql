// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.MenuBarItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0
    horizontalPadding: Theme.space.m
    font: Theme.font.ui
    hoverEnabled: true

    contentItem: Text {
        text: control.text.replace(/&&/g, "\u0001").replace(/&/g, "").replace(/\u0001/g, "&")
        font: control.font
        color: control.enabled ? Theme.color.textPrimary : Theme.color.textDisabled
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitHeight: Theme.size.control - Theme.space.xs
        y: (control.height - height) / 2
        height: Theme.size.control - Theme.space.xs
        radius: Theme.radius.control
        color: control.down || control.highlighted ? Theme.color.bgHover : "transparent"
    }
}
