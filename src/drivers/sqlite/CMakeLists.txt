# SPDX-License-Identifier: GPL-3.0-or-later
# SQLite driver: a static library (also used directly by unit tests) wrapped as a plugin.
qt_add_library(owl_sqlite STATIC
    sqlitedialect.h sqlitedialect.cpp
    sqlitedriver.h sqlitedriver.cpp)
target_include_directories(owl_sqlite PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owl_sqlite PUBLIC Owl::Core PRIVATE SQLite::SQLite3)
set_target_properties(owl_sqlite PROPERTIES POSITION_INDEPENDENT_CODE ON)
owl_set_target_options(owl_sqlite)

owl_add_driver_plugin(owlsql_driver_sqlite SOURCES sqliteplugin.cpp LIBS owl_sqlite)
