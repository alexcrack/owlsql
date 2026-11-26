# SPDX-License-Identifier: GPL-3.0-or-later
# QML module OwlSql.App — C++ application layer exposed to QML (commands, later sessions,
# workspace, settings...). See docs/architecture.md.

qt_add_library(owlsql_app STATIC)

qt_add_qml_module(owlsql_app
    URI OwlSql.App
    VERSION 1.0
    SOURCES
        appcontext.h appcontext.cpp
        appdatabase.h appdatabase.cpp
        apppaths.h apppaths.cpp
        commandregistry.h commandregistry.cpp
        keybindings.h keybindings.cpp
        settings.h settings.cpp
        singleinstance.h singleinstance.cpp
        uistate.h uistate.cpp
        watchedfile.h watchedfile.cpp
    QML_FILES
        qml/CommandAction.qml
)

file(GLOB owl_migrations CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/migrations/owlsql/*.sql")
qt_add_resources(owlsql_app "owlsql_app_migrations"
    PREFIX "/owlsql"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${owl_migrations})

target_include_directories(owlsql_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owlsql_app
    PUBLIC Qt6::Gui Qt6::Qml Qt6::Sql
    PRIVATE Qt6::Network Qt6::Quick Qt6::QuickControls2 owlsql_theme)
owl_set_target_options(owlsql_app)
