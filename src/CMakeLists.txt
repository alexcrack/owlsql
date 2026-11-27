# SPDX-License-Identifier: GPL-3.0-or-later
add_subdirectory(core)
add_subdirectory(drivers)
add_subdirectory(theme)
add_subdirectory(style)
add_subdirectory(app)
add_subdirectory(ui)

# The OwlSql executable, with its app icon (decision 175; regenerate with tools/make-app-icons.sh).
set(owl_appicon_dir "${CMAKE_SOURCE_DIR}/resources/appicon")
qt_add_executable(owlsql main.cpp)
if(WIN32)
    target_sources(owlsql PRIVATE owlsql.rc)
elseif(APPLE)
    set(owl_icns "${owl_appicon_dir}/owlsql.icns")
    set_source_files_properties("${owl_icns}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(owlsql PRIVATE "${owl_icns}")
    set_target_properties(owlsql PROPERTIES MACOSX_BUNDLE_ICON_FILE owlsql.icns)
endif()

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

# Linux desktop integration: launcher entry and hicolor theme icons.
if(UNIX AND NOT APPLE)
    install(FILES "${owl_appicon_dir}/org.owlsql.OwlSql.desktop"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    foreach(size 16 24 32 48 64 128 256 512)
        install(FILES "${owl_appicon_dir}/png/owlsql-${size}.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
            RENAME org.owlsql.OwlSql.png)
    endforeach()
    install(FILES "${CMAKE_SOURCE_DIR}/resources/logo/owlsql-app.svg"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME org.owlsql.OwlSql.svg)
endif()
