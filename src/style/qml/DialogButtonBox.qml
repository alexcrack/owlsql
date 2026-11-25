// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.DialogButtonBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, (control.count === 1
                                                                               ? implicitContentWidth
                                                                                 * 2 : implicitContentWidth)
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    spacing: Theme.space.m
    padding: Theme.space.xl
    topPadding: Theme.space.m
    alignment: Qt.AlignRight

    delegate: Button {
        variant: T.DialogButtonBox.buttonRole === T.DialogButtonBox.AcceptRole
                 || T.DialogButtonBox.buttonRole === T.DialogButtonBox.YesRole ? "primary" :
                                                                                 "secondary"
    }

    contentItem: ListView {
        implicitWidth: contentWidth
        model: control.contentModel
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
    }

    background: Item {}
}
