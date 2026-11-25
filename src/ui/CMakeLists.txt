# SPDX-License-Identifier: GPL-3.0-or-later
# QML module OwlSql.Ui — application screens and components.

qt_add_library(owlsql_ui STATIC)

qt_add_qml_module(owlsql_ui
    URI OwlSql.Ui
    VERSION 1.0
    IMPORTS OwlSql.Theme OwlSql.Style
    QML_FILES
        qml/Main.qml
        qml/Gallery.qml
)

target_link_libraries(owlsql_ui PRIVATE Qt6::Quick Qt6::QuickControls2 owlsql_theme owlsql_style)
owl_set_target_options(owlsql_ui)
