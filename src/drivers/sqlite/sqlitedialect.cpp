// SPDX-License-Identifier: GPL-3.0-or-later
#include "sqlitedialect.h"

namespace owl::sqlite {

namespace {
SqlLexerOptions sqliteOptions()
{
    SqlLexerOptions o;
    o.backtickIdentifiers = true;
    o.bracketIdentifiers = true;
    o.questionPlaceholders = true;
    o.namedPlaceholders = true;
    return o;
}
} // namespace

SqliteDialect::SqliteDialect()
    : SqlDialect(sqliteOptions())
{ }

} // namespace owl::sqlite
