// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

// In-window menu bar (Windows/Linux). On macOS Qt uses the native global menu bar instead.
T.MenuBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    leftPadding: Theme.space.xs
    font: Theme.font.ui

    delegate: MenuBarItem {}

    contentItem: Row {
        spacing: control.spacing

        Repeater {
            model: control.contentModel
        }
    }

    background: Rectangle {
        implicitHeight: Theme.size.control
        color: Theme.color.bgSurface

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.color.borderSubtle
        }
    }
}
