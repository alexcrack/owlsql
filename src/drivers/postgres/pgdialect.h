// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/dialect.h"

namespace owl::postgres {

// PostgreSQL SQL: $tag$ dollar quotes, nested /* */ comments, E'…' escapes, $1 placeholders
// (`?` and `:name` are operators / casts / slices here), BEGIN ATOMIC function bodies,
// SELECT … INTO creates a table, bytea literals as '\x…'::bytea.
class PgDialect : public SqlDialect
{
public:
    PgDialect();

    QString quoteLiteral(const Value &value) const override;
    StatementClass classify(QStringView statement) const override;

protected:
    bool opensCompoundBody(const QList<QStringView> &leadingWords) const override;
    QString quoteBytes(const QByteArray &bytes) const override;
};

} // namespace owl::postgres
