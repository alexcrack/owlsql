# SPDX-License-Identifier: GPL-3.0-or-later
# PostgreSQL driver (libpq): a static library (also used directly by unit tests) wrapped as a plugin.
qt_add_library(owl_postgres STATIC
    pgdialect.h pgdialect.cpp
    pgvalues.h pgvalues.cpp
    pgdriver.h pgdriver.cpp)
target_include_directories(owl_postgres PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owl_postgres PUBLIC Owl::Core PRIVATE owl_driver_common PostgreSQL::PostgreSQL)
set_target_properties(owl_postgres PROPERTIES POSITION_INDEPENDENT_CODE ON)
owl_set_target_options(owl_postgres)

owl_add_driver_plugin(owlsql_driver_postgres SOURCES pgplugin.cpp LIBS owl_postgres)
