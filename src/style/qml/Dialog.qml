// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding, implicitHeaderWidth, implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding + (
                                 implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0) + (
                                 implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    padding: Theme.space.xl
    topPadding: Theme.space.s
    font: Theme.font.ui

    background: PopupBackground {}

    header: Label {
        text: control.title
        visible: control.title.length > 0
        elide: Label.ElideRight
        font: Theme.font.title
        leftPadding: Theme.space.xl
        rightPadding: Theme.space.xl
        topPadding: Theme.space.l
        bottomPadding: Theme.space.xs
    }

    footer: DialogButtonBox {
        visible: count > 0
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.motion.normal
        }
        NumberAnimation {
            property: "scale"
            from: 0.97
            to: 1
            duration: Theme.motion.normal
            easing.type: Easing.OutCubic
        }
    }

    T.Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.4)
    }
    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
