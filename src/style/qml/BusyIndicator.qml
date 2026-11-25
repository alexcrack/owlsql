// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import QtQuick.Templates as T
import OwlSql.Theme

T.BusyIndicator {
    id: control

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: implicitContentHeight + topPadding + bottomPadding

    padding: 0

    contentItem: Item {
        implicitWidth: 20
        implicitHeight: 20
        opacity: control.running ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            OpacityAnimator {
                duration: Theme.motion.normal
            }
        }

        Shape {
            id: spinner

            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                strokeColor: Theme.color.accentFill
                strokeWidth: 2
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap

                PathAngleArc {
                    centerX: spinner.width / 2
                    centerY: spinner.height / 2
                    radiusX: spinner.width / 2 - 2
                    radiusY: spinner.height / 2 - 2
                    startAngle: 0
                    sweepAngle: 270
                }
            }

            RotationAnimator on rotation {
                running: control.running && control.visible
                from: 0
                to: 360
                duration: 900
                loops: Animation.Infinite
            }
        }
    }
}
