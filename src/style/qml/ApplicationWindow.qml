// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.ApplicationWindow {
    color: Theme.color.bgCanvas
    font: Theme.font.ui

    // Palette for anything still drawn by the fallback (Basic) style.
    palette {
        window: Theme.color.bgSurface
        windowText: Theme.color.textPrimary
        base: Theme.color.bgRaised
        alternateBase: Theme.color.bgSunken
        text: Theme.color.textPrimary
        button: Theme.color.bgRaised
        buttonText: Theme.color.textPrimary
        brightText: Theme.color.accentOnFill
        highlight: Theme.color.accentFill
        highlightedText: Theme.color.accentOnFill
        placeholderText: Theme.color.textMuted
        toolTipBase: Theme.color.bgRaised
        toolTipText: Theme.color.textPrimary
        light: Theme.color.bgHover
        midlight: Theme.color.bgHover
        mid: Theme.color.borderDefault
        dark: Theme.color.borderStrong
        shadow: Theme.color.shadow
        link: Theme.color.accentText
    }
}
