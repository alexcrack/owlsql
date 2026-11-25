// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T
import OwlSql.Theme

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset, implicitContentWidth
                            + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 0
    leftPadding: Theme.space.m
    rightPadding: Theme.space.m + (indicator ? indicator.width + Theme.space.xs : 0)
    font: Theme.font.ui
    hoverEnabled: true

    delegate: ItemDelegate {
        required property var model
        required property int index

        width: ListView.view.width
        text: model[control.textRole]
        font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }

    indicator: OwlIcon {
        x: control.width - width - Theme.space.m
        y: control.topPadding + (control.availableHeight - height) / 2
        name: "chevron-down"
        color: control.enabled ? Theme.color.textSecondary : Theme.color.textDisabled
    }

    contentItem: T.TextField {
        text: control.editable ? control.editText : control.displayText
        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse
        font: control.font
        color: control.enabled ? Theme.color.textPrimary : Theme.color.textDisabled
        selectionColor: Theme.color.bgSelected
        selectedTextColor: Theme.color.textPrimary
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        implicitWidth: 140
        implicitHeight: Theme.size.control
        radius: Theme.radius.control
        color: !control.enabled ? Theme.color.bgSunken : control.down
                                  ? Theme.color.bgSelectedInactive : control.hovered &&
                                    !control.editable ? Theme.color.bgHover : Theme.color.bgRaised
        border.color: control.activeFocus && control.editable ? Theme.color.accentLine :
                                                                control.hovered
                                                                ? Theme.color.borderStrong :
                                                                  Theme.color.borderDefault

        FocusRing {
            visible: control.visualFocus && !control.editable
        }
    }

    popup: T.Popup {
        y: control.height + Theme.space.xs
        width: control.width
        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, (
                             control.T.Overlay.overlay ? control.T.Overlay.overlay.height : 480)
                         - topMargin - bottomMargin)
        topMargin: Theme.space.m
        bottomMargin: Theme.space.m
        padding: Theme.space.xs

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            T.ScrollBar.vertical: ScrollBar {}
        }

        background: PopupBackground {}

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Theme.motion.fast
            }
        }
    }
}
