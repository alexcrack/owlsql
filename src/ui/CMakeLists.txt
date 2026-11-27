# SPDX-License-Identifier: GPL-3.0-or-later
# QML module OwlSql.Ui — application screens and components.

qt_add_library(owlsql_ui STATIC)

qt_add_qml_module(owlsql_ui
    URI OwlSql.Ui
    VERSION 1.0
    IMPORTS OwlSql.Theme OwlSql.Style OwlSql.App
    QML_FILES
        qml/Main.qml
        qml/Gallery.qml
        qml/AboutDialog.qml
        qml/BottomPanel.qml
        qml/EmptyState.qml
        qml/KeyChip.qml
        qml/Sidebar.qml
        qml/StatusBar.qml
        qml/WelcomePage.qml
        qml/WorkspaceTabBar.qml
        qml/ConfirmDialog.qml
        qml/ConnectionField.qml
        qml/ImportDialog.qml
        qml/NameDialog.qml
        qml/PasswordDialog.qml
        qml/SafetyBadge.qml
        qml/SessionEditor.qml
        qml/SessionList.qml
        qml/SessionRow.qml
        qml/TagDot.qml
        qml/TagSwatch.qml
)

target_link_libraries(owlsql_ui PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 owlsql_theme owlsql_style owlsql_app)
owl_set_target_options(owlsql_ui)
