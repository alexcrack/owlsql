// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.MenuSeparator {
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    verticalPadding: Theme.space.xs
    horizontalPadding: Theme.space.xs

    contentItem: Rectangle {
        implicitWidth: 180
        implicitHeight: 1
        color: Theme.color.borderSubtle
    }
}
