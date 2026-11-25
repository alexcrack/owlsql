// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

// Internal: icon + text content shared by buttons, tool buttons, delegates and menu items.
// Implicit size comes from the parts' implicit sizes only (never from our own width), so
// controls size to their content; when made narrower, the text elides.
Item {
    id: root

    property string text
    property string iconName
    property color color: Theme.color.textPrimary
    property font font: Theme.font.ui
    property int iconSize: Theme.size.icon
    property int spacing: Theme.space.s
    property int display: T.AbstractButton.TextBesideIcon
    property int alignment: Qt.AlignCenter

    // "&File" -> "File", "&&" -> "&" (mnemonic markers are not drawn).
    readonly property string _plainText: text.replace(/&&/g, "\u0001").replace(/&/g, "").replace(
                                             /\u0001/g, "&")

    readonly property bool _showIcon: iconName.length > 0 && display !== T.AbstractButton.TextOnly
    readonly property bool _showText: text.length > 0 && display !== T.AbstractButton.IconOnly
    readonly property real _iconPart: _showIcon ? iconSize + (_showText ? spacing : 0) : 0

    implicitWidth: _iconPart + (_showText ? label.implicitWidth : 0)
    implicitHeight: Math.max(_showIcon ? iconSize : 0, _showText ? label.implicitHeight : 0)

    readonly property real _usedWidth: Math.min(width, implicitWidth)
    readonly property real _startX: alignment & Qt.AlignLeft ? 0 : (width - _usedWidth) / 2

    OwlIcon {
        x: root._startX
        anchors.verticalCenter: parent.verticalCenter
        visible: root._showIcon
        name: root.iconName
        color: root.color
        size: root.iconSize
    }

    Text {
        id: label

        x: root._startX + root._iconPart
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(0, root._usedWidth - root._iconPart)
        visible: root._showText
        text: root.text
        font: root.font
        color: root.color
        elide: Text.ElideRight
    }
}
