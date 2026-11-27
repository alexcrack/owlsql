// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import OwlSql.Theme

// A session's colour tag (design-system.md §3.4): one of red, orange, amber, green, teal, blue,
// violet, pink.
Rectangle {
    id: root

    property string tag: "green"
    property int diameter: 8

    function tagColor(name: string): color {
        const key = "tag" + name.charAt(0).toUpperCase() + name.slice(1);
        return Theme.color[key] ?? Theme.color.textMuted;
    }

    implicitWidth: diameter
    implicitHeight: diameter
    radius: diameter / 2
    color: tagColor(tag)
    Accessible.ignored: true
}
