// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

// 1px divider with a wider invisible grab area (Theme.size.splitterHit).
T.SplitView {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    handle: Rectangle {
        id: handleDelegate

        implicitWidth: control.orientation === Qt.Horizontal ? 1 : control.width
        implicitHeight: control.orientation === Qt.Horizontal ? control.height : 1
        color: T.SplitHandle.pressed || T.SplitHandle.hovered ? Theme.color.accentLine :
                                                                Theme.color.borderDefault

        containmentMask: Item {
            x: control.orientation === Qt.Horizontal ? (handleDelegate.width - width) / 2 : 0
            y: control.orientation === Qt.Horizontal ? 0 : (handleDelegate.height - height) / 2
            width: control.orientation === Qt.Horizontal ? Theme.size.splitterHit :
                                                           handleDelegate.width
            height: control.orientation === Qt.Horizontal ? handleDelegate.height :
                                                            Theme.size.splitterHit
        }
    }
}
