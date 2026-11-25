// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.Frame {
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, contentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset, contentHeight
                             + topPadding + bottomPadding)

    padding: Theme.space.l
    font: Theme.font.ui

    background: Rectangle {
        color: Theme.color.bgSurface
        radius: Theme.radius.popup
        border.color: Theme.color.borderDefault
    }
}
