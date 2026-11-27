# SPDX-License-Identifier: GPL-3.0-or-later
qt_add_library(owl_conformance STATIC conformance.h conformance.cpp)
target_include_directories(owl_conformance PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(owl_conformance PUBLIC Owl::Core Qt6::Test)

# owl_add_conformance_test(<name> <source> <driver plugin target>)
function(owl_add_conformance_test name source plugin)
    owl_add_unit_test(${name} SOURCES ${source} LIBS owl_conformance owl_test_server)
    target_compile_definitions(${name} PRIVATE "OWL_DRIVER_DIR=\"${OWL_DRIVER_OUTPUT_DIR}\"")
    add_dependencies(${name} ${plugin})
endfunction()

# SQLite: runs everywhere, no server needed.
owl_add_conformance_test(tst_conformance_sqlite tst_conformance_sqlite.cpp owlsql_driver_sqlite)
set_tests_properties(tst_conformance_sqlite PROPERTIES LABELS conformance)

# Server engines: skipped without a server URL (tests/support/testserver.h). CI runs each against
# the newest and the oldest supported server (decisions 048, 151, 161).
owl_add_conformance_test(tst_conformance_pg tst_conformance_pg.cpp owlsql_driver_postgres)
owl_add_server_runs(tst_conformance_pg LABELS conformance
    RUNS tst_conformance_pg13=OWL_TEST_PG13_URL)

owl_add_conformance_test(tst_conformance_mysql tst_conformance_mysql.cpp owlsql_driver_mysql)
owl_add_server_runs(tst_conformance_mysql LABELS conformance RUNS
    tst_conformance_mysql57=OWL_TEST_MYSQL57_URL
    tst_conformance_mariadb=OWL_TEST_MARIADB_URL
    tst_conformance_mariadb106=OWL_TEST_MARIADB106_URL)
