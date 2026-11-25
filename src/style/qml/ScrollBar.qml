// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

// Thin overlay scrollbar that widens on hover and fades out when idle.
T.ScrollBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: 2
    hoverEnabled: true
    visible: control.policy !== T.ScrollBar.AlwaysOff
    minimumSize: orientation === Qt.Horizontal ? height / width : width / height

    contentItem: Rectangle {
        readonly property real thickness: control.hovered || control.pressed ? 8 : 5

        implicitWidth: control.interactive ? thickness : 2
        implicitHeight: control.interactive ? thickness : 2
        radius: Math.min(width, height) / 2
        color: control.pressed ? Theme.color.textSecondary : Theme.color.textMuted
        opacity: 0

        Behavior on implicitWidth {
            NumberAnimation {
                duration: Theme.motion.fast
            }
        }
        Behavior on implicitHeight {
            NumberAnimation {
                duration: Theme.motion.fast
            }
        }

        states: State {
            name: "active"
            when: control.policy === T.ScrollBar.AlwaysOn || (control.active && control.size < 1.0)
            PropertyChanges {
                control.contentItem.opacity: 0.7
            }
        }

        transitions: Transition {
            from: "active"
            SequentialAnimation {
                PauseAnimation {
                    duration: 600
                }
                NumberAnimation {
                    target: control.contentItem
                    property: "opacity"
                    to: 0
                    duration: Theme.motion.normal
                }
            }
        }
    }
}
