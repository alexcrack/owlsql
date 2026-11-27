# SPDX-License-Identifier: GPL-3.0-or-later
# QML module OwlSql.Theme — design tokens (Theme singleton), bundled fonts and icons.

qt_add_library(owlsql_theme STATIC)

qt_add_qml_module(owlsql_theme
    URI OwlSql.Theme
    VERSION 1.0
    SOURCES
        themetokens.h
        thememanager.h thememanager.cpp
        iconprovider.h iconprovider.cpp
    QML_FILES
        qml/OwlIcon.qml
)

set(owl_resources_dir "${PROJECT_SOURCE_DIR}/resources")
file(GLOB owl_icon_files CONFIGURE_DEPENDS
    "${owl_resources_dir}/icons/tabler/*.svg")

qt_add_resources(owlsql_theme "owlsql_theme_assets"
    PREFIX "/owlsql"
    BASE "${owl_resources_dir}"
    FILES
        "${owl_resources_dir}/themes/metrics.json"
        "${owl_resources_dir}/themes/owl-dark.json"
        "${owl_resources_dir}/themes/owl-light.json"
        "${owl_resources_dir}/fonts/inter/Inter-Regular.ttf"
        "${owl_resources_dir}/fonts/inter/Inter-Medium.ttf"
        "${owl_resources_dir}/fonts/inter/Inter-SemiBold.ttf"
        "${owl_resources_dir}/fonts/inter/Inter-Italic.ttf"
        "${owl_resources_dir}/fonts/jetbrains-mono/JetBrainsMono-Regular.ttf"
        "${owl_resources_dir}/fonts/jetbrains-mono/JetBrainsMono-Bold.ttf"
        "${owl_resources_dir}/fonts/jetbrains-mono/JetBrainsMono-Italic.ttf"
        "${owl_resources_dir}/logo/owlsql.svg"
        "${owl_resources_dir}/logo/owlsql-small.svg"
        "${owl_resources_dir}/appicon/png/owlsql-16.png"
        "${owl_resources_dir}/appicon/png/owlsql-24.png"
        "${owl_resources_dir}/appicon/png/owlsql-32.png"
        "${owl_resources_dir}/appicon/png/owlsql-48.png"
        "${owl_resources_dir}/appicon/png/owlsql-64.png"
        "${owl_resources_dir}/appicon/png/owlsql-128.png"
        "${owl_resources_dir}/appicon/png/owlsql-256.png"
        ${owl_icon_files}
)

target_include_directories(owlsql_theme PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owlsql_theme PUBLIC Qt6::Gui Qt6::Qml Qt6::Quick PRIVATE Qt6::Svg)
owl_set_target_options(owlsql_theme)
