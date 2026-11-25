// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

// Underline-style tab bar (decision 061).
T.TabBar {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, contentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset, contentHeight
                             + topPadding + bottomPadding)

    spacing: Theme.space.xs

    contentItem: ListView {
        model: control.contentModel
        currentIndex: control.currentIndex
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        snapMode: ListView.SnapToItem
        highlightMoveDuration: 0
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: 40
        preferredHighlightEnd: width - 40
    }

    background: Item {
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.color.borderSubtle
        }
    }
}
