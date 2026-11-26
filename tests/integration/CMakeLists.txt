# SPDX-License-Identifier: GPL-3.0-or-later
# Tests against real database servers. They skip themselves unless the server URL variables are
# set (OWL_TEST_PG_URL); CI provides a PostgreSQL service on the Linux job (decision 125).
# Run only these: ctest -L integration
owl_add_unit_test(tst_pg_connect SOURCES tst_pg_connect.cpp LIBS PostgreSQL::PostgreSQL)
set_tests_properties(tst_pg_connect PROPERTIES LABELS integration)
