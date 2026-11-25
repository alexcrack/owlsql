// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

// Square ghost button for toolbars: ToolButton { icon.name: "refresh"; ToolTip.text: qsTr("Refresh") }
T.ToolButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.space.s
    spacing: Theme.space.s
    font: Theme.font.ui
    hoverEnabled: true

    ToolTip.visible: hovered && ToolTip.text.length > 0
    ToolTip.delay: 600

    contentItem: ButtonContent {
        text: control.text
        iconName: control.icon.name
        display: control.display
        font: control.font
        spacing: control.spacing
        color: !control.enabled ? Theme.color.textDisabled : control.checked
                                  ? Theme.color.accentText : control.hovered
                                    ? Theme.color.textPrimary : Theme.color.textSecondary
    }

    background: Rectangle {
        implicitWidth: Theme.size.control
        implicitHeight: Theme.size.control
        radius: Theme.radius.control
        color: control.down ? Theme.color.bgSelectedInactive : control.checked
                              ? Theme.color.bgSelected : control.hovered ? Theme.color.bgHover :
                                                                           "transparent"

        FocusRing {
            visible: control.visualFocus
        }
    }
}
