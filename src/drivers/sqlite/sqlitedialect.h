// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/dialect.h"

namespace owl::sqlite {

// SQLite SQL: "x", `x` and [x] identifiers; ?, ?NNN, :name, @name, $name placeholders;
// CREATE TRIGGER bodies (BEGIN … END) contain semicolons.
class SqliteDialect : public SqlDialect
{
public:
    SqliteDialect();
};

} // namespace owl::sqlite
