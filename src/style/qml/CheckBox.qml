// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.CheckBox {
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
        readonly property bool on: control.checkState !== Qt.Unchecked

        implicitWidth: 16
        implicitHeight: 16
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding :
                                              control.leftPadding) : control.leftPadding + (
                              control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 3
        opacity: control.enabled ? 1 : 0.5
        color: on ? (control.down ? Theme.color.accentFillPressed : control.hovered
                                    ? Theme.color.accentFillHover : Theme.color.accentFill) :
                    Theme.color.bgRaised
        border.width: on ? 0 : 1
        border.color: control.hovered ? Theme.color.borderStrong : Theme.color.borderDefault

        OwlIcon {
            anchors.centerIn: parent
            visible: parent.on
            name: control.checkState === Qt.PartiallyChecked ? "minus" : "check"
            color: Theme.color.accentOnFill
            size: 14
            stroke: 3
        }

        FocusRing {
            visible: control.visualFocus
            baseRadius: 3
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
