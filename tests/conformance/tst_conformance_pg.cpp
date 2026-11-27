// SPDX-License-Identifier: GPL-3.0-or-later
// The driver conformance suite against a PostgreSQL server (tests/support/testserver.h).
#include "conformance.h"
#include "testserver.h"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    const owl::test::TestServer server = owl::test::testServer("OWL_TEST_PG_URL", u"postgres"_s);
    owl::conformance::Fixtures f;
    f.driverId = u"postgres"_s;
    f.pluginDirectory = QStringLiteral(OWL_DRIVER_DIR);
    f.params = server.params;
    f.skipReason = server.skipReason;
    f.setupError = server.error;

    f.selectScalars
        = uR"(SELECT 42, 4611686018427387904, -7, 2.5::float8, 'Ωmega 🦉', NULL, '', '\x00ff10'::bytea)"_s;
    f.generateRows = [](int n) {
        return u"SELECT x FROM generate_series(1, %1) AS x"_s.arg(n);
    };
    f.bigText = [](int n) {
        return u"SELECT repeat('x', %1)"_s.arg(n);
    };
    f.runaway = u"SELECT pg_sleep(60)"_s;
    f.selectThreeParams = u"SELECT $1::int8, $2::text, $3::text"_s;
    f.syntaxError = u"SELECT 1 FROM WHERE"_s;
    f.syntaxErrorPosition = 14; // "WHERE"
    f.createUniqueTable = u"CREATE TEMP TABLE owl_conf (x integer UNIQUE)"_s;
    f.insertThreeRows = u"INSERT INTO owl_conf VALUES (1), (2), (3)"_s;
    f.insertDuplicate = u"INSERT INTO owl_conf VALUES (1)"_s;
    f.updateAllRows = u"UPDATE owl_conf SET x = x + 10"_s;
    f.writeStatement = u"CREATE TEMP TABLE owl_conf_ro (x integer)"_s;
    return owl::conformance::runConformance(argc, argv, f);
}
