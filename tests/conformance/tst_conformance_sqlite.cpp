// SPDX-License-Identifier: GPL-3.0-or-later
#include "conformance.h"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    owl::conformance::Fixtures f;
    f.driverId = u"sqlite"_s;
    f.pluginDirectory = QStringLiteral(OWL_DRIVER_DIR);
    f.params.driverId = f.driverId;
    f.params.database = u":memory:"_s;

    f.selectScalars
        = uR"(SELECT 42, 4611686018427387904, -7, 2.5, 'Ωmega 🦉', NULL, '', X'00FF10')"_s;
    f.generateRows = [](int n) {
        return u"WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x + 1 FROM c LIMIT %1) "
               u"SELECT x FROM c"_s.arg(n);
    };
    f.bigText = [](int n) {
        return u"SELECT printf('%.*c', %1, 'x')"_s.arg(n);
    };
    f.runaway = u"WITH RECURSIVE c(x) AS (SELECT 1 UNION ALL SELECT x + 1 FROM c) "
                u"SELECT count(*) FROM c"_s;
    f.selectThreeParams = u"SELECT ?, ?, ?"_s;
    f.syntaxError = u"SELECT 1 FROM WHERE"_s;
    f.syntaxErrorPosition = 14; // "WHERE"
    f.createUniqueTable = u"CREATE TEMP TABLE owl_conf (x INTEGER UNIQUE)"_s;
    f.insertThreeRows = u"INSERT INTO owl_conf VALUES (1), (2), (3)"_s;
    f.insertDuplicate = u"INSERT INTO owl_conf VALUES (1)"_s;
    f.updateAllRows = u"UPDATE owl_conf SET x = x + 10"_s;
    f.writeStatement = u"CREATE TEMP TABLE owl_conf_ro (x INTEGER)"_s;
    f.createWalkTable = u"CREATE TABLE owl_conf_walk (id INTEGER PRIMARY KEY)"_s;
    f.dropWalkTable = u"DROP TABLE IF EXISTS owl_conf_walk"_s;
    return owl::conformance::runConformance(argc, argv, f);
}
