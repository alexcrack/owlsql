// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

// Button { text: qsTr("Run"); variant: "primary"; icon.name: "player-play" }
T.Button {
    id: control

    // "primary" | "secondary" | "ghost" | "danger"
    property string variant: highlighted ? "primary" : flat ? "ghost" : "secondary"
    property bool small: false

    readonly property color _fg: {
        if (variant === "primary")
            return Theme.color.accentOnFill;
        if (variant === "danger")
            return Theme.color.danger;
        return checked ? Theme.color.accentText : Theme.color.textPrimary;
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 0
    horizontalPadding: small ? Theme.space.m : Theme.space.l
    spacing: Theme.space.s
    font: small ? Theme.font.uiSmall : Theme.font.ui
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45

    contentItem: ButtonContent {
        text: control.text
        iconName: control.icon.name
        display: control.display
        color: control._fg
        font: control.font
        spacing: control.spacing
        iconSize: control.small ? Theme.size.iconSmall : Theme.size.icon
    }

    background: Rectangle {
        implicitWidth: control.small ? 0 : 64
        implicitHeight: control.small ? Theme.size.controlSmall : Theme.size.control
        radius: Theme.radius.control
        color: {
            switch (control.variant) {
            case "primary":
                return control.down ? Theme.color.accentFillPressed : control.hovered
                                      ? Theme.color.accentFillHover : Theme.color.accentFill;
            case "ghost":
                return control.down ? Theme.color.bgSelectedInactive : control.checked
                                      ? Theme.color.bgSelected : control.hovered
                                        ? Theme.color.bgHover : "transparent";
            case "danger":
                return control.down ? Theme.color.dangerSubtle : control.hovered
                                      ? Theme.color.bgHover : Theme.color.bgRaised;
            default:
                return control.down ? Theme.color.bgSelectedInactive : control.checked
                                      ? Theme.color.bgSelected : control.hovered
                                        ? Theme.color.bgHover : Theme.color.bgRaised;
            }
        }
        border.width: control.variant === "primary" || control.variant === "ghost" ? 0 : 1
        border.color: control.variant === "danger" && control.hovered ? Theme.color.danger :
                                                                        control.hovered
                                                                        ? Theme.color.borderStrong :
                                                                          Theme.color.borderDefault

        FocusRing {
            visible: control.visualFocus
        }
    }
}
