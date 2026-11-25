// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window

// A Tabler outline icon, tinted and rendered crisply at the screen's pixel density.
// OwlIcon { name: "table"; color: Theme.color.textPrimary }
Image {
    id: root

    property string name
    property color color: Theme.color.textSecondary
    property int size: Theme.size.icon
    property real stroke: 2

    readonly property real _pixelRatio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1

    width: size
    height: size
    sourceSize: Qt.size(Math.ceil(size * _pixelRatio), Math.ceil(size * _pixelRatio))
    fillMode: Image.PreserveAspectFit
    smooth: true
    source: name.length > 0 ? "image://owlicon/%1?color=%2&stroke=%3".arg(name).arg(
                                  encodeURIComponent(root.color.toString())).arg(stroke) : ""
}
