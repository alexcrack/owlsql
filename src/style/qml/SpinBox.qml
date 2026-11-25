// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.SpinBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0
    leftPadding: Theme.space.m
    rightPadding: (up.indicator ? up.indicator.width : 0) + Theme.space.xs
    font: Theme.font.ui
    editable: true
    hoverEnabled: true

    // Plain integers (ports, limits, row counts): no locale digit grouping.
    textFromValue: (value, locale) => Number(value).toString()
    valueFromText: (text, locale) => parseInt(text, 10)

    validator: IntValidator {
        locale: control.locale.name
        bottom: Math.min(control.from, control.to)
        top: Math.max(control.from, control.to)
    }

    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.color.textPrimary : Theme.color.textDisabled
        selectionColor: Theme.color.bgSelected
        selectedTextColor: Theme.color.textPrimary
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        clip: width < implicitWidth
    }

    up.indicator: Rectangle {
        x: control.width - width - 1
        y: 1
        implicitWidth: 20
        implicitHeight: (control.height - 2) / 2
        radius: Theme.radius.control - 1
        color: control.up.pressed ? Theme.color.bgSelectedInactive : control.up.hovered
                                    ? Theme.color.bgHover : "transparent"

        OwlIcon {
            anchors.centerIn: parent
            name: "chevron-up"
            size: Theme.size.iconSmall - 2
            color: control.up.indicator.enabled ? Theme.color.textSecondary :
                                                  Theme.color.textDisabled
        }
    }

    down.indicator: Rectangle {
        x: control.width - width - 1
        y: control.height / 2
        implicitWidth: 20
        implicitHeight: (control.height - 2) / 2
        radius: Theme.radius.control - 1
        color: control.down.pressed ? Theme.color.bgSelectedInactive : control.down.hovered
                                      ? Theme.color.bgHover : "transparent"

        OwlIcon {
            anchors.centerIn: parent
            name: "chevron-down"
            size: Theme.size.iconSmall - 2
            color: control.down.indicator.enabled ? Theme.color.textSecondary :
                                                    Theme.color.textDisabled
        }
    }

    background: Rectangle {
        implicitWidth: 96
        implicitHeight: Theme.size.control
        radius: Theme.radius.control
        color: control.enabled ? Theme.color.bgRaised : Theme.color.bgSunken
        border.color: control.activeFocus ? Theme.color.accentLine : control.hovered ? Theme.color.borderStrong :
                                                                                       Theme.color.borderDefault
    }
}
