# SPDX-License-Identifier: GPL-3.0-or-later
# Tests against real database servers. They skip themselves unless their server URL variable is set
# (tests/support/testserver.h); CI provides PostgreSQL 18 and 13, MySQL 8.4 and 5.7, MariaDB 11.8
# and 10.6 on the Linux job (decisions 125, 151, 161). Run only these: ctest -L integration
owl_add_unit_test(tst_pg_driver SOURCES tst_pg_driver.cpp LIBS owl_postgres owl_test_server)
owl_add_server_runs(tst_pg_driver RUNS tst_pg_driver13=OWL_TEST_PG13_URL)

owl_add_unit_test(tst_mysql_driver SOURCES tst_mysql_driver.cpp LIBS owl_mysql owl_test_server)
owl_add_server_runs(tst_mysql_driver RUNS
    tst_mysql_driver57=OWL_TEST_MYSQL57_URL
    tst_mariadb_driver=OWL_TEST_MARIADB_URL
    tst_mariadb_driver106=OWL_TEST_MARIADB106_URL)

owl_add_unit_test(tst_pg_catalog SOURCES tst_pg_catalog.cpp LIBS owl_postgres owl_test_server)
owl_add_server_runs(tst_pg_catalog RUNS tst_pg_catalog13=OWL_TEST_PG13_URL)

owl_add_unit_test(tst_mysql_catalog SOURCES tst_mysql_catalog.cpp LIBS owl_mysql owl_test_server)
owl_add_server_runs(tst_mysql_catalog RUNS
    tst_mysql_catalog57=OWL_TEST_MYSQL57_URL
    tst_mariadb_catalog=OWL_TEST_MARIADB_URL
    tst_mariadb_catalog106=OWL_TEST_MARIADB106_URL)

# Runtime sessions on PostgreSQL and MySQL (needs both URLs; each part skips on its own).
owl_add_unit_test(tst_session_servers SOURCES tst_session_servers.cpp
    LIBS owlsql_app owl_postgres owl_mysql owl_test_server)
set_tests_properties(tst_session_servers PROPERTIES LABELS integration)
