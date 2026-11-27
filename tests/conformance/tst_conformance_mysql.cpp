// SPDX-License-Identifier: GPL-3.0-or-later
// The driver conformance suite against a MySQL or MariaDB server (tests/support/testserver.h).
// The SQL avoids CTEs and window functions: MySQL 5.7 has neither.
#include "conformance.h"
#include "testserver.h"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    const owl::test::TestServer server = owl::test::testServer("OWL_TEST_MYSQL_URL", u"mysql"_s);
    owl::conformance::Fixtures f;
    f.driverId = u"mysql"_s;
    f.pluginDirectory = QStringLiteral(OWL_DRIVER_DIR);
    f.params = server.params;
    f.skipReason = server.skipReason;
    f.setupError = server.error;

    f.selectScalars
        = u"SELECT 42, 4611686018427387904, -7, 2.5e0, 'Ωmega 🦉', NULL, '', X'00FF10'"_s;
    f.generateRows = [](int n) {
        const QString digit = u"(SELECT 0 AS d UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL "
                              u"SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 "
                              u"UNION ALL SELECT 7 UNION ALL SELECT 8 UNION ALL SELECT 9)"_s;
        return u"SELECT x FROM (SELECT a.d + 10 * b.d + 100 * c.d + 1000 * e.d + 10000 * g.d + 1 "
               u"AS x FROM %1 a, %1 b, %1 c, %1 e, %1 g) t WHERE x <= %2 ORDER BY x"_s.arg(digit)
                   .arg(n);
    };
    f.bigText = [](int n) {
        return u"SELECT REPEAT('x', %1)"_s.arg(n);
    };
    f.runaway = u"SELECT SLEEP(60)"_s;
    f.selectThreeParams = u"SELECT CAST(? AS SIGNED), CAST(? AS CHAR), ?"_s;
    f.syntaxError = u"SELECT 1 FROM WHERE"_s;
    f.syntaxErrorPosition = 14; // "WHERE"
    f.createUniqueTable = u"CREATE TEMPORARY TABLE owl_conf (x INT UNIQUE)"_s;
    f.insertThreeRows = u"INSERT INTO owl_conf VALUES (1), (2), (3)"_s;
    f.insertDuplicate = u"INSERT INTO owl_conf VALUES (1)"_s;
    f.updateAllRows = u"UPDATE owl_conf SET x = x + 10"_s;
    f.writeStatement = u"CREATE TEMPORARY TABLE owl_conf_ro (x INT)"_s;
    f.createWalkTable = u"CREATE TABLE owl_conf_walk (id INT PRIMARY KEY)"_s;
    f.dropWalkTable = u"DROP TABLE IF EXISTS owl_conf_walk"_s;
    return owl::conformance::runConformance(argc, argv, f);
}
