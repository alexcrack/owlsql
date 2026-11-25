// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import OwlSql.App
import OwlSql.Style
import OwlSql.Theme

// Application shell: sidebar | workspace tabs / bottom panel, menu bar and status bar
// (docs/specs/design-system.md §8.1). All actions go through the command registry.
ApplicationWindow {
    id: window

    property bool sidebarVisible: true
    property bool bottomPanelVisible: false // collapsed by default (decision 068)
    readonly property int tabCount: tabsModel.count
    property int currentTab: 0

    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 500
    visible: true
    title: tabCount > 0 ? qsTr("%1 — OwlSql").arg(tabsModel.get(currentTab).title) : "OwlSql"

    function handleCommand(id: string) {
        switch (id) {
        case "app.quit":
            Qt.quit();
            break;
        case "view.toggleSidebar":
            sidebarVisible = !sidebarVisible;
            break;
        case "view.toggleBottomPanel":
            bottomPanelVisible = !bottomPanelVisible;
            break;
        case "view.toggleTheme":
            Theme.mode = Theme.dark ? Theme.Light : Theme.Dark;
            break;
        case "view.themeSystem":
            Theme.mode = Theme.System;
            break;
        case "view.themeLight":
            Theme.mode = Theme.Light;
            break;
        case "view.themeDark":
            Theme.mode = Theme.Dark;
            break;
        case "workspace.showWelcome":
            showWelcome();
            break;
        case "workspace.closeTab":
            closeTab(currentTab);
            break;
        case "workspace.nextTab":
            if (tabCount > 0)
                currentTab = (currentTab + 1) % tabCount;
            break;
        case "workspace.previousTab":
            if (tabCount > 0)
                currentTab = (currentTab - 1 + tabCount) % tabCount;
            break;
        case "help.about":
            aboutDialog.open();
            break;
        }
    }

    function showWelcome() {
        for (let i = 0; i < tabsModel.count; ++i) {
            if (tabsModel.get(i).kind === "welcome") {
                currentTab = i;
                return;
            }
        }
        tabsModel.insert(0, {
                             kind: "welcome",
                             title: qsTr("Welcome"),
                             icon: "home"
                         });
        currentTab = 0;
    }

    function closeTab(index: int) {
        if (index < 0 || index >= tabsModel.count)
            return;
        tabsModel.remove(index);
        currentTab = Math.min(currentTab, tabsModel.count - 1);
    }

    Connections {
        target: Commands

        function onTriggered(id: string) {
            window.handleCommand(id);
        }
    }

    ListModel {
        id: tabsModel

        ListElement {
            kind: "welcome"
            title: qsTr("Welcome")
            icon: "home"
        }
    }

    // Window-level actions own the shortcuts; menu items reuse them.
    CommandAction {
        id: quitAction

        command: "app.quit"
    }
    CommandAction {
        id: toggleSidebarAction

        command: "view.toggleSidebar"
        checkable: true
        checked: window.sidebarVisible
    }
    CommandAction {
        id: toggleBottomPanelAction

        command: "view.toggleBottomPanel"
        checkable: true
        checked: window.bottomPanelVisible
    }
    CommandAction {
        id: toggleThemeAction

        command: "view.toggleTheme"
    }
    CommandAction {
        id: themeSystemAction

        command: "view.themeSystem"
        checkable: true
        checked: Theme.mode === Theme.System
    }
    CommandAction {
        id: themeLightAction

        command: "view.themeLight"
        checkable: true
        checked: Theme.mode === Theme.Light
    }
    CommandAction {
        id: themeDarkAction

        command: "view.themeDark"
        checkable: true
        checked: Theme.mode === Theme.Dark
    }
    CommandAction {
        id: showWelcomeAction

        command: "workspace.showWelcome"
    }
    CommandAction {
        id: closeTabAction

        command: "workspace.closeTab"
        enabled: window.tabCount > 0
    }
    CommandAction {
        id: nextTabAction

        command: "workspace.nextTab"
    }
    CommandAction {
        id: previousTabAction

        command: "workspace.previousTab"
    }
    CommandAction {
        id: aboutAction

        command: "help.about"
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")

            MenuItem {
                action: closeTabAction
            }
            MenuSeparator {}
            MenuItem {
                action: quitAction
            }
        }

        Menu {
            title: qsTr("&View")

            MenuItem {
                action: toggleSidebarAction
            }
            MenuItem {
                action: toggleBottomPanelAction
            }
            MenuSeparator {}
            Menu {
                title: qsTr("Theme")

                MenuItem {
                    action: themeSystemAction
                }
                MenuItem {
                    action: themeLightAction
                }
                MenuItem {
                    action: themeDarkAction
                }
                MenuSeparator {}
                MenuItem {
                    action: toggleThemeAction
                }
            }
            MenuSeparator {}
            MenuItem {
                action: showWelcomeAction
            }
        }

        Menu {
            title: qsTr("&Help")

            MenuItem {
                action: aboutAction
            }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        Sidebar {
            visible: window.sidebarVisible
            SplitView.preferredWidth: 260
            SplitView.minimumWidth: 180
            SplitView.maximumWidth: 600
        }

        SplitView {
            SplitView.fillWidth: true
            orientation: Qt.Vertical

            ColumnLayout {
                SplitView.fillHeight: true
                spacing: 0

                WorkspaceTabBar {
                    Layout.fillWidth: true
                    model: tabsModel
                    currentIndex: window.currentTab
                    onActivated: index => window.currentTab = index
                    onCloseRequested: index => window.closeTab(index)
                }

                WelcomePage {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.tabCount > 0 && tabsModel.get(window.currentTab).kind
                             === "welcome"

                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: window.tabCount === 0
                    color: Theme.color.bgCanvas

                    EmptyState {
                        anchors.fill: parent
                        iconName: "layout-sidebar"
                        title: qsTr("No open tabs")
                        text: qsTr("Open a connection or show the Welcome tab.")
                        actionText: Commands.title("workspace.showWelcome")
                        onActionTriggered: Commands.trigger("workspace.showWelcome")
                    }
                }
            }

            BottomPanel {
                visible: window.bottomPanelVisible
                SplitView.preferredHeight: 200
                SplitView.minimumHeight: 120
            }
        }
    }

    footer: StatusBar {}

    AboutDialog {
        id: aboutDialog

        anchors.centerIn: parent
    }
}
