# SPDX-License-Identifier: GPL-3.0-or-later
# MySQL / MariaDB driver (MariaDB Connector/C): a static library (also used directly by unit tests)
# wrapped as a plugin.
qt_add_library(owl_mysql STATIC
    mysqldialect.h mysqldialect.cpp
    mysqlvalues.h mysqlvalues.cpp
    mysqlcatalog.h mysqlcatalog.cpp
    mysqldriver.h mysqldriver.cpp)
target_include_directories(owl_mysql PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owl_mysql PUBLIC Owl::Core PRIVATE owl_driver_common unofficial::libmariadb)
set_target_properties(owl_mysql PROPERTIES POSITION_INDEPENDENT_CODE ON)
owl_set_target_options(owl_mysql)

owl_add_driver_plugin(owlsql_driver_mysql SOURCES mysqlplugin.cpp LIBS owl_mysql)
