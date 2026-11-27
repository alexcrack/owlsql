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

    // Initial values come from the remembered UI state; changes are saved back (UiState batches
    // writes into owlsql.db).
    property bool sidebarVisible: UiState.value("layout.sidebarVisible", true)
    property bool bottomPanelVisible: UiState.value("layout.bottomPanelVisible",
                                                    false) // decision 068
    readonly property int tabCount: tabsModel.count
    property int currentTab: 0

    width: UiState.value("window.width", 1280)
    height: UiState.value("window.height", 800)
    minimumWidth: 800
    minimumHeight: 500
    visible: true
    title: tabsModel.get(currentTab)?.title ? qsTr("%1 — OwlSql").arg(tabsModel.get(currentTab).title) :
                                              "OwlSql"

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
            Settings.setValue("theme.mode", Theme.dark ? "light" : "dark");
            break;
        case "view.themeSystem":
            Settings.setValue("theme.mode", "system");
            break;
        case "view.themeLight":
            Settings.setValue("theme.mode", "light");
            break;
        case "view.themeDark":
            Settings.setValue("theme.mode", "dark");
            break;
        case "workspace.showWelcome":
            showWelcome();
            break;
        case "session.new":
            showWelcome();
            welcomePage.startNew("");
            break;
        case "session.import":
            showWelcome();
            welcomePage.startImport();
            break;
        case "session.disconnect":
            Sessions.close(Sessions.activeSession);
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
        saveTabs();
    }

    function saveTabs() {
        const kinds = [];
        for (let i = 0; i < tabsModel.count; ++i)
            kinds.push(tabsModel.get(i).kind);
        UiState.setValue("workspace.tabs", kinds);
    }

    function closeTab(index: int) {
        if (index < 0 || index >= tabsModel.count)
            return;
        tabsModel.remove(index);
        currentTab = Math.min(currentTab, tabsModel.count - 1);
        saveTabs();
    }

    onSidebarVisibleChanged: UiState.setValue("layout.sidebarVisible", sidebarVisible)
    onBottomPanelVisibleChanged: UiState.setValue("layout.bottomPanelVisible", bottomPanelVisible)
    onWidthChanged: if (visibility === Window.Windowed)
                        UiState.setValue("window.width", width)
    onHeightChanged: if (visibility === Window.Windowed)
                         UiState.setValue("window.height", height)
    onVisibilityChanged: () => {
        if (window.visibility === Window.Maximized || window.visibility === Window.Windowed)
            UiState.setValue("window.maximized", window.visibility === Window.Maximized);
    }

    Component.onCompleted: {
        const tabs = UiState.value("workspace.tabs", ["welcome"]);
        if (!tabs.includes("welcome"))
            tabsModel.clear();
        if (UiState.value("window.maximized", false))
            showMaximized();
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
        id: newSessionAction

        command: "session.new"
        enabled: Sessions.available
    }
    CommandAction {
        id: importSessionsAction

        command: "session.import"
        enabled: Sessions.available
    }
    CommandAction {
        id: disconnectAction

        command: "session.disconnect"
        enabled: Sessions.activeSession !== ""
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
                action: newSessionAction
            }
            MenuItem {
                action: importSessionsAction
            }
            MenuItem {
                action: disconnectAction
            }
            MenuSeparator {}
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
        onResizingChanged: if (!resizing)
                               UiState.setValue("layout.sidebarWidth", sidebar.width)

        Sidebar {
            id: sidebar

            visible: window.sidebarVisible
            SplitView.preferredWidth: UiState.value("layout.sidebarWidth", 260)
            SplitView.minimumWidth: 180
            SplitView.maximumWidth: 600
        }

        SplitView {
            SplitView.fillWidth: true
            orientation: Qt.Vertical
            onResizingChanged: if (!resizing)
                                   UiState.setValue("layout.bottomPanelHeight", bottomPanel.height)

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
                    id: welcomePage

                    objectName: "welcomePage"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: tabsModel.get(window.currentTab)?.kind === "welcome"
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
                id: bottomPanel

                visible: window.bottomPanelVisible
                SplitView.preferredHeight: UiState.value("layout.bottomPanelHeight", 200)
                SplitView.minimumHeight: 120
            }
        }
    }

    footer: StatusBar {}

    // Prod session active: a 2 px danger frame around the window content (decision 063).
    Rectangle {
        anchors.fill: parent
        z: 1000
        visible: Sessions.activeInfo.level === "prod" && ["connecting", "connected"].includes(
                     Sessions.activeInfo.state)
        color: "transparent"
        border.width: 2
        border.color: Theme.color.danger
        Accessible.ignored: true
    }

    AboutDialog {
        id: aboutDialog

        anchors.centerIn: parent
    }

    PasswordDialog {
        id: passwordDialog

        anchors.centerIn: parent
    }

    Connections {
        target: Sessions

        function onPasswordRequested(sessionId: string, name: string) {
            passwordDialog.ask(sessionId, name);
        }
    }
}
