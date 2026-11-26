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

# PostgreSQL: needs a server (OWL_TEST_PG_URL); skipped without one. CI runs it against the
# latest server and, through OWL_TEST_PG_URL_VAR, against the oldest supported one (decision 048).
owl_add_unit_test(tst_conformance_pg SOURCES tst_conformance_pg.cpp
    LIBS owl_conformance owl_test_pgserver)
target_compile_definitions(tst_conformance_pg PRIVATE
    "OWL_DRIVER_DIR=\"${OWL_DRIVER_OUTPUT_DIR}\"")
add_dependencies(tst_conformance_pg owlsql_driver_postgres)
add_test(NAME tst_conformance_pg13 COMMAND tst_conformance_pg)
set_tests_properties(tst_conformance_pg PROPERTIES LABELS "conformance;integration")
set_tests_properties(tst_conformance_pg13 PROPERTIES LABELS "conformance;integration"
    ENVIRONMENT "OWL_TEST_PG_URL_VAR=OWL_TEST_PG13_URL")
