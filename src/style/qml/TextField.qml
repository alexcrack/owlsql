// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

// TextField { placeholderText: qsTr("Host"); error: !valid }
T.TextField {
    id: control

    property bool error: false

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset || Math.max(contentWidth,
                                                                                placeholder.implicitWidth)
                   + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset, contentHeight
                             + topPadding + bottomPadding, placeholder.implicitHeight + topPadding
                             + bottomPadding)

    leftPadding: Theme.space.m
    rightPadding: Theme.space.m
    topPadding: Theme.space.xs
    bottomPadding: Theme.space.xs
    verticalAlignment: TextInput.AlignVCenter
    hoverEnabled: true

    font: Theme.font.ui
    color: enabled ? Theme.color.textPrimary : Theme.color.textDisabled
    placeholderTextColor: Theme.color.textMuted
    selectionColor: Theme.color.bgSelected
    selectedTextColor: Theme.color.textPrimary

    Text {
        id: placeholder

        x: control.leftPadding
        y: control.topPadding
        width: control.width - control.leftPadding - control.rightPadding
        height: control.height - control.topPadding - control.bottomPadding
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        elide: Text.ElideRight
        visible: !control.length && !control.preeditText
    }

    background: Rectangle {
        implicitWidth: 180
        implicitHeight: Theme.size.control
        radius: Theme.radius.control
        color: control.enabled ? Theme.color.bgRaised : Theme.color.bgSunken
        border.width: 1
        border.color: control.error ? Theme.color.danger : control.activeFocus ? Theme.color.accentLine :
                                                                                 control.hovered
                                                                                 ? Theme.color.borderStrong :
                                                                                   Theme.color.borderDefault
    }
}
