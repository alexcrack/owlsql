// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.TextArea {
    id: control

    implicitWidth: Math.max(contentWidth + leftPadding + rightPadding, implicitBackgroundWidth
                            + leftInset + rightInset, placeholder.implicitWidth + leftPadding
                            + rightPadding)
    implicitHeight: Math.max(contentHeight + topPadding + bottomPadding, implicitBackgroundHeight
                             + topInset + bottomInset, placeholder.implicitHeight + topPadding
                             + bottomPadding)

    padding: Theme.space.m
    hoverEnabled: true
    font: Theme.font.ui
    color: enabled ? Theme.color.textPrimary : Theme.color.textDisabled
    placeholderTextColor: Theme.color.textMuted
    selectionColor: Theme.color.bgSelected
    selectedTextColor: Theme.color.textPrimary
    wrapMode: TextEdit.Wrap

    Text {
        id: placeholder

        x: control.leftPadding
        y: control.topPadding
        width: control.width - control.leftPadding - control.rightPadding
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        wrapMode: Text.Wrap
        visible: !control.length && !control.preeditText
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 64
        radius: Theme.radius.control
        color: control.enabled ? Theme.color.bgRaised : Theme.color.bgSunken
        border.color: control.activeFocus ? Theme.color.accentLine : control.hovered ? Theme.color.borderStrong :
                                                                                       Theme.color.borderDefault
    }
}
