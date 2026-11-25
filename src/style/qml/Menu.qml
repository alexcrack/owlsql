// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.Menu {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: 0
    padding: Theme.space.xs
    overlap: Theme.space.xs
    font: Theme.font.ui

    delegate: MenuItem {}

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: Window.window ? contentHeight + control.topPadding + control.bottomPadding
                                     > control.height : false
        clip: true
        currentIndex: control.currentIndex

        T.ScrollBar.vertical: ScrollBar {}
    }

    background: PopupBackground {
        implicitWidth: 200
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.motion.fast
        }
    }

    T.Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.4)
    }
    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
