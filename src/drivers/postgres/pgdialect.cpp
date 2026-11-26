// SPDX-License-Identifier: GPL-3.0-or-later
#include "pgdialect.h"

#include <cmath>

namespace owl::postgres {

namespace {

SqlLexerOptions pgOptions()
{
    SqlLexerOptions o;
    o.dollarQuotes = true;
    o.nestedBlockComments = true;
    o.questionPlaceholders = false; // jsonb ?, ?| and ?& operators
    o.dollarNumberPlaceholders = true;
    o.namedPlaceholders = false; // ::casts and arr[1:2] slices
    return o;
}

bool isWord(QStringView sql, const SqlToken &t, QLatin1StringView word)
{
    return t.type == SqlToken::Type::Word && t.text(sql).compare(word, Qt::CaseInsensitive) == 0;
}

} // namespace

PgDialect::PgDialect()
    : SqlDialect(pgOptions())
{ }

QString PgDialect::quoteLiteral(const Value &value) const
{
    if (value.kind() == ValueKind::Double) {
        const double d = value.toDouble();
        if (std::isnan(d))
            return QStringLiteral("'NaN'::float8");
        if (std::isinf(d))
            return d > 0 ? QStringLiteral("'Infinity'::float8")
                         : QStringLiteral("'-Infinity'::float8");
    }
    return SqlDialect::quoteLiteral(value);
}

StatementClass PgDialect::classify(QStringView statement) const
{
    const StatementClass base = SqlDialect::classify(statement);
    if (base != StatementClass::Read)
        return base;
    // SELECT … INTO new_table (also inside WITH … SELECT) creates a table.
    const QList<SqlToken> tokens = tokenize(statement);
    int depth = 0;
    bool selectSeen = false;
    for (const SqlToken &t : tokens) {
        if (t.type == SqlToken::Type::OpenParen)
            ++depth;
        else if (t.type == SqlToken::Type::CloseParen)
            depth = qMax(0, depth - 1);
        else if (depth == 0 && isWord(statement, t, QLatin1StringView("SELECT")))
            selectSeen = true;
        else if (depth == 0 && selectSeen && isWord(statement, t, QLatin1StringView("INTO")))
            return StatementClass::Ddl;
    }
    return base;
}

bool PgDialect::opensCompoundBody(const QList<QStringView> &leadingWords) const
{
    Q_UNUSED(leadingWords)
    return false; // PostgreSQL triggers call functions; BEGIN ATOMIC is handled by SqlDialect
}

QString PgDialect::quoteBytes(const QByteArray &bytes) const
{
    return QStringLiteral("'\\x") + QString::fromLatin1(bytes.toHex()) + QStringLiteral("'::bytea");
}

} // namespace owl::postgres
