// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.MenuItem {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 0
    // Space for the check mark (left) and submenu arrow (right) keeps labels aligned in a menu.
    leftPadding: Theme.space.m + (control.checkable ? Theme.size.icon + spacing : 0)
    rightPadding: Theme.space.m + (control.subMenu ? Theme.size.icon + spacing : 0)
    spacing: Theme.space.m
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

    indicator: OwlIcon {
        x: Theme.space.m
        y: control.topPadding + (control.availableHeight - height) / 2
        visible: control.checkable && control.checked
        name: "check"
        color: Theme.color.accentText
    }

    arrow: OwlIcon {
        x: control.width - width - Theme.space.m
        y: control.topPadding + (control.availableHeight - height) / 2
        visible: control.subMenu !== null
        name: "chevron-right"
        color: Theme.color.textSecondary
    }

    background: Rectangle {
        implicitWidth: 180
        implicitHeight: Theme.size.row + Theme.space.xs
        radius: Theme.radius.control
        color: control.down ? Theme.color.bgSelectedInactive : control.highlighted
                              ? Theme.color.bgHover : "transparent"
    }
}
