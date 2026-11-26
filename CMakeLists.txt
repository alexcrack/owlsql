# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.30) # KSyntaxHighlighting (built at configure time) needs 3.30

# C libraries (libpq, sqlite3) come from the vcpkg manifest when VCPKG_ROOT is set (decision 124).
# Its builtin-baseline in vcpkg.json pins the versions; the checkout only needs to contain it.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
endif()
# Windows: static vcpkg libraries with the dynamic CRT (/MD) that Qt uses — no extra DLLs to ship.
if(CMAKE_HOST_WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static-md" CACHE STRING "")
endif()

project(OwlSql
    VERSION 0.1.0
    DESCRIPTION "Database client for PostgreSQL, MySQL/MariaDB and SQLite"
    HOMEPAGE_URL "https://github.com/alexcrack/owlsql"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# We don't use C++20 modules. CMake >= 3.28 would otherwise scan every source for them with GCC 14+
# (extra build step, and -fmodule-mapper flags in compile_commands.json that clang-tidy rejects).
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(OWL_BUILD_TESTS "Build unit tests" ON)
option(OWL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(OwlCompilerOptions)

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Sql Network LinguistTools)
qt_standard_project_setup(REQUIRES 6.12 I18N_SOURCE_LANGUAGE en)
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

include(OwlDependencies)

add_subdirectory(src)

# Translations (decision 027): `cmake --build <dir> --target update_translations` runs lupdate.
# English is the source language, so i18n/owlsql_en.ts only carries plural forms; real
# translations are added to I18N_TRANSLATED_LANGUAGES above.
qt_add_translations(TARGETS owlsql
    TS_FILE_BASE owlsql
    TS_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/i18n")

if(OWL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
