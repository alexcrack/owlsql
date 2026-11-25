# SPDX-License-Identifier: GPL-3.0-or-later

qt_add_executable(owlsql main.cpp)

target_link_libraries(owlsql PRIVATE
    Owl::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    owlsql_uiplugin)

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
