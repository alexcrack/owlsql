// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/dialect.h"

namespace owl::mysql {

// MySQL / MariaDB SQL: `x` identifiers, "x" strings, backslash escapes, # and "-- " comments,
// ? placeholders. Scripts may change the statement delimiter with the client command
// "DELIMITER //" (as in the mysql CLI and HeidiSQL); without it, stored-program bodies
// (CREATE PROCEDURE / FUNCTION / TRIGGER / EVENT … BEGIN … END, BEGIN NOT ATOMIC) keep their
// semicolons. SELECT … INTO OUTFILE / DUMPFILE writes a file.
class MySqlDialect : public SqlDialect
{
public:
    MySqlDialect();

    QString quoteIdentifier(QStringView name) const override;
    QString quoteLiteral(const Value &value) const override;
    QList<StatementSpan> split(QStringView script) const override;
    StatementClass classify(QStringView statement) const override;
};

} // namespace owl::mysql
