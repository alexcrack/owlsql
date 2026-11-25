// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: Theme.space.xxs
    spacing: Theme.space.m
    font: Theme.font.ui
    hoverEnabled: true

    indicator: Rectangle {
        implicitWidth: 28
        implicitHeight: 16
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding :
                                              control.leftPadding) : control.leftPadding + (
                              control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: height / 2
        opacity: control.enabled ? 1 : 0.5
        color: control.checked ? (control.hovered ? Theme.color.accentFillHover :
                                                    Theme.color.accentFill) : control.hovered
                                 ? Theme.color.bgSelectedInactive : Theme.color.bgHover
        border.width: control.checked ? 0 : 1
        border.color: Theme.color.borderDefault

        Rectangle {
            x: Math.max(2, Math.min(parent.width - width - 2, control.visualPosition * parent.width
                                    - width / 2))
            anchors.verticalCenter: parent.verticalCenter
            width: 12
            height: 12
            radius: 6
            color: control.checked ? Theme.color.accentOnFill : Theme.color.textSecondary

            Behavior on x {
                enabled: !control.down
                NumberAnimation {
                    duration: Theme.motion.fast
                    easing.type: Easing.OutCubic
                }
            }
        }

        FocusRing {
            visible: control.visualFocus
            baseRadius: 8
        }
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width
                                                              + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width
                                                              + control.spacing : 0
        text: control.text
        font: control.font
        color: control.enabled ? Theme.color.textPrimary : Theme.color.textDisabled
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
