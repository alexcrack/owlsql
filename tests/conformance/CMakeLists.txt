# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(owl_conformance STATIC conformance.h conformance.cpp)
target_include_directories(owl_conformance PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owl_conformance PUBLIC Owl::Core Qt6::Test)

# SQLite: runs everywhere, no server needed.
owl_add_unit_test(tst_conformance_sqlite SOURCES tst_conformance_sqlite.cpp LIBS owl_conformance)
target_compile_definitions(tst_conformance_sqlite PRIVATE
    "OWL_DRIVER_DIR=\"${OWL_DRIVER_OUTPUT_DIR}\"")
add_dependencies(tst_conformance_sqlite owlsql_driver_sqlite)
set_tests_properties(tst_conformance_sqlite PROPERTIES LABELS conformance)
