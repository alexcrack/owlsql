# SPDX-License-Identifier: GPL-3.0-or-later
# QML module OwlSql.App — C++ application layer exposed to QML (commands, later sessions,
# workspace, settings...). See docs/architecture.md.

qt_add_library(owlsql_app STATIC)

qt_add_qml_module(owlsql_app
    URI OwlSql.App
    VERSION 1.0
    SOURCES
        commandregistry.h commandregistry.cpp
    QML_FILES
        qml/CommandAction.qml
)

target_include_directories(owlsql_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owlsql_app PUBLIC Qt6::Gui Qt6::Qml PRIVATE Qt6::Quick Qt6::QuickControls2)
owl_set_target_options(owlsql_app)
