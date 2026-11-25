// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.TabButton {
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
        font: control.font
        spacing: control.spacing
        color: !control.enabled ? Theme.color.textDisabled : control.checked || control.hovered
                                  ? Theme.color.textPrimary : Theme.color.textSecondary
    }

    background: Item {
        implicitHeight: Theme.size.tabBar

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            radius: 1
            color: Theme.color.accentLine
            visible: control.checked
        }

        FocusRing {
            visible: control.visualFocus
        }
    }
}
