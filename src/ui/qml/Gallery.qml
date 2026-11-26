// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Development-only component gallery: `owlsql --gallery`. Every control in every state.
ApplicationWindow {
    id: window

    width: 1180
    height: 900
    visible: true
    title: qsTr("OwlSql — Component Gallery")

    component Section: ColumnLayout {
        id: section

        required property string title
        default property alias content: body.data

        Layout.fillWidth: true
        spacing: Theme.space.m

        Label {
            text: section.title
            font: Theme.font.title
        }

        Flow {
            id: body

            Layout.fillWidth: true
            spacing: Theme.space.m
        }
    }

    header: Rectangle {
        implicitHeight: Theme.size.toolbar + Theme.space.m
        color: Theme.color.bgSurface

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.color.borderDefault
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space.xl
            anchors.rightMargin: Theme.space.xl
            spacing: Theme.space.l

            Label {
                text: qsTr("Component Gallery")
                font: Theme.font.title
            }

            Label {
                text: Theme.name
                color: Theme.color.textMuted
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Theme")
                color: Theme.color.textSecondary
            }

            ComboBox {
                model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                currentIndex: Theme.mode === Theme.Light ? 1 : Theme.mode === Theme.Dark ? 2 : 0
                onActivated: index => Settings.setValue("theme.mode", ["system", "light",
                                                                       "dark"][index]) ? Theme.Dark :
                                                                                         Theme.System
            }

            Switch {
                text: qsTr("Reduce motion")
                checked: Theme.reduceMotion
                onToggled: Settings.setValue("ui.reduceMotion", checked)
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width
            spacing: Theme.space.xxl

            Item {
                implicitHeight: Theme.space.xs
            }

            Section {
                title: qsTr("Buttons")
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                Button {
                    text: qsTr("Run")
                    variant: "primary"
                    icon.name: "player-play"
                }
                Button {
                    text: qsTr("Secondary")
                }
                Button {
                    text: qsTr("Ghost")
                    variant: "ghost"
                }
                Button {
                    text: qsTr("Drop table")
                    variant: "danger"
                    icon.name: "trash"
                }
                Button {
                    text: qsTr("Toggle")
                    checkable: true
                    checked: true
                }
                Button {
                    text: qsTr("Disabled")
                    enabled: false
                }
                Button {
                    text: qsTr("Disabled primary")
                    variant: "primary"
                    enabled: false
                }
                Button {
                    text: qsTr("Small")
                    small: true
                }
                Button {
                    text: qsTr("Small primary")
                    variant: "primary"
                    small: true
                    icon.name: "device-floppy"
                }
                Button {
                    icon.name: "settings"
                    display: Button.IconOnly
                }
            }

            Section {
                title: qsTr("Tool buttons")
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                ToolButton {
                    icon.name: "refresh"
                    ToolTip.text: qsTr("Refresh (F5)")
                }
                ToolButton {
                    icon.name: "filter"
                    checkable: true
                    checked: true
                    ToolTip.text: qsTr("Filter")
                }
                ToolButton {
                    icon.name: "layout-sidebar"
                }
                ToolButton {
                    icon.name: "layout-bottombar"
                }
                ToolButton {
                    icon.name: "history"
                    text: qsTr("History")
                }
                ToolButton {
                    icon.name: "trash"
                    enabled: false
                }
            }

            Section {
                title: qsTr("Inputs")
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                TextField {
                    placeholderText: qsTr("Host")
                }
                TextField {
                    text: "db.internal"
                }
                TextField {
                    text: "70000"
                    error: true
                }
                TextField {
                    text: qsTr("Disabled")
                    enabled: false
                }
                SpinBox {
                    from: 1
                    to: 65535
                    value: 5432
                }
                ComboBox {
                    model: ["PostgreSQL", "MySQL", "MariaDB", "SQLite"]
                }
                ComboBox {
                    editable: true
                    model: ["public", "app", "analytics"]
                }
                TextArea {
                    width: 320
                    placeholderText: qsTr("Notes for this session…")
                }
            }

            Section {
                title: qsTr("Toggles")
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                CheckBox {
                    text: qsTr("Checked")
                    checked: true
                }
                CheckBox {
                    text: qsTr("Unchecked")
                }
                CheckBox {
                    text: qsTr("Partial")
                    tristate: true
                    checkState: Qt.PartiallyChecked
                }
                CheckBox {
                    text: qsTr("Disabled")
                    checked: true
                    enabled: false
                }
                RadioButton {
                    text: qsTr("dev")
                }
                RadioButton {
                    text: qsTr("staging")
                }
                RadioButton {
                    text: qsTr("prod")
                    checked: true
                }
                Switch {
                    text: qsTr("Auto-commit")
                    checked: true
                }
                Switch {
                    text: qsTr("Read-only")
                }
            }

            Section {
                title: qsTr("Progress")
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                ProgressBar {
                    width: 200
                    value: 0.6
                }
                ProgressBar {
                    width: 200
                    indeterminate: true
                }
                BusyIndicator {
                    running: true
                }
            }

            Section {
                title: qsTr("Tabs")
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                TabBar {
                    width: 520

                    TabButton {
                        text: qsTr("Data")
                        width: implicitWidth
                    }
                    TabButton {
                        text: qsTr("Structure")
                        width: implicitWidth
                    }
                    TabButton {
                        text: qsTr("Indexes")
                        width: implicitWidth
                    }
                    TabButton {
                        text: qsTr("DDL")
                        width: implicitWidth
                    }
                }
            }

            Section {
                title: qsTr("Menus, popups and dialogs")
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                Button {
                    id: menuButton

                    text: qsTr("Open menu")
                    icon.name: "dots-vertical"
                    onClicked: menu.popup(menuButton, 0, menuButton.height + Theme.space.xs)

                    Menu {
                        id: menu

                        objectName: "galleryMenu"

                        MenuItem {
                            text: qsTr("Copy")
                            icon.name: "copy"
                        }
                        MenuItem {
                            text: qsTr("Word wrap")
                            checkable: true
                            checked: true
                        }
                        Menu {
                            title: qsTr("Copy as")

                            MenuItem {
                                text: "CSV"
                            }
                            MenuItem {
                                text: "JSON"
                            }
                            MenuItem {
                                text: "SQL INSERT"
                            }
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: qsTr("Delete")
                            icon.name: "trash"
                        }
                        MenuItem {
                            text: qsTr("Disabled")
                            enabled: false
                        }
                    }
                }
                Button {
                    text: qsTr("Open dialog")
                    onClicked: dialog.open()
                }
                Button {
                    text: qsTr("Hover for tooltip")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Tooltips use the OwlSql style")
                }
            }

            Section {
                title: qsTr("Split view and frame")
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                Frame {
                    width: 520
                    height: 140
                    padding: 0

                    SplitView {
                        anchors.fill: parent

                        Pane {
                            SplitView.preferredWidth: 180

                            Label {
                                text: qsTr("Tree")
                                color: Theme.color.textSecondary
                            }
                        }
                        Pane {
                            Label {
                                text: qsTr("Workspace — drag the divider")
                                color: Theme.color.textSecondary
                            }
                        }
                    }
                }
            }

            Section {
                title: qsTr("Icons (%1)").arg(iconRepeater.count)
                Layout.leftMargin: Theme.space.xl
                Layout.rightMargin: Theme.space.xl

                Repeater {
                    id: iconRepeater

                    model: Theme.iconNames()

                    delegate: Rectangle {
                        id: iconCell

                        required property string modelData

                        width: 36
                        height: 36
                        radius: Theme.radius.control
                        color: iconHover.hovered ? Theme.color.bgHover : "transparent"

                        OwlIcon {
                            anchors.centerIn: parent
                            name: iconCell.modelData
                            color: Theme.color.textPrimary
                        }

                        HoverHandler {
                            id: iconHover
                        }

                        ToolTip.visible: iconHover.hovered
                        ToolTip.text: iconCell.modelData
                    }
                }
            }

            Item {
                implicitHeight: Theme.space.xl
            }
        }
    }

    Dialog {
        id: dialog

        objectName: "galleryDialog"

        anchors.centerIn: parent
        width: 420
        modal: true
        title: qsTr("Drop table users?")
        standardButtons: Dialog.Ok | Dialog.Cancel

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTr(
                      "This permanently deletes the table and all of its rows. This cannot be undone.")
            color: Theme.color.textSecondary
        }
    }
}
