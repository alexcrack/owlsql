# SPDX-License-Identifier: GPL-3.0-or-later
add_subdirectory(core)
add_subdirectory(theme)
add_subdirectory(style)
add_subdirectory(app)
add_subdirectory(ui)

# The OwlSql executable.
qt_add_executable(owlsql main.cpp)

target_link_libraries(owlsql PRIVATE
    Owl::Core
    owlsql_theme
    owlsql_themeplugin
    owlsql_styleplugin
    owlsql_app
    owlsql_appplugin
    owlsql_uiplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2)

owl_set_target_options(owlsql)

set_target_properties(owlsql PROPERTIES
    OUTPUT_NAME owlsql
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER org.owlsql.OwlSql
    MACOSX_BUNDLE_BUNDLE_NAME OwlSql
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

include(GNUInstallDirs)
install(TARGETS owlsql
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
