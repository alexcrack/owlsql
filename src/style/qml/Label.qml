// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.Label {
    color: enabled ? Theme.color.textPrimary : Theme.color.textDisabled
    linkColor: Theme.color.accentText
    font: Theme.font.ui
    verticalAlignment: Text.AlignVCenter
}
