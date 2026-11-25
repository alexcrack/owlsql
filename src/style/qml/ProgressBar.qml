// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.ProgressBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    contentItem: Item {
        implicitWidth: 160
        implicitHeight: 4
        clip: true

        Rectangle {
            visible: !control.indeterminate
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.color.accentFill
        }

        Rectangle {
            id: bar

            visible: control.indeterminate
            width: parent.width * 0.3
            height: parent.height
            radius: 2
            color: Theme.color.accentFill

            XAnimator on x {
                running: control.indeterminate && control.visible
                from: -bar.width
                to: bar.parent.width
                duration: 1200
                loops: Animation.Infinite
            }
        }
    }

    background: Rectangle {
        implicitWidth: 160
        implicitHeight: 4
        y: (control.height - height) / 2
        height: 4
        radius: 2
        color: Theme.color.bgHover
    }
}
