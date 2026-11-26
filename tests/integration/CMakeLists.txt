# SPDX-License-Identifier: GPL-3.0-or-later
# Tests against real database servers. They skip themselves unless the server URL variables are
# set (OWL_TEST_PG_URL, see tests/support/pgtestserver.h); CI provides PostgreSQL 18 and 13 on the
# Linux job (decision 125). Run only these: ctest -L integration
owl_add_unit_test(tst_pg_driver SOURCES tst_pg_driver.cpp LIBS owl_postgres owl_test_pgserver)
add_test(NAME tst_pg_driver13 COMMAND tst_pg_driver)
set_tests_properties(tst_pg_driver PROPERTIES LABELS integration)
set_tests_properties(tst_pg_driver13 PROPERTIES LABELS integration
    ENVIRONMENT "OWL_TEST_PG_URL_VAR=OWL_TEST_PG13_URL")
