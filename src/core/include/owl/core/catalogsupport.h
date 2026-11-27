// SPDX-License-Identifier: GPL-3.0-or-later
// Helpers for driver catalogs: run an internal query to the end and read its cells as text /
// numbers / booleans, whatever kind the engine reported.
#pragma once

#include "owl/core/driver.h"

namespace owl::catalog {

using Row = QList<Value>;

// Runs `sql` (StatementOrigin::Internal) and returns all rows of its first result.
Result<QList<Row>> query(IConnection &connection, const QString &sql, QList<Value> params = {});

QString text(const Value &v); // NULL → ""
qint64 integer(const Value &v); // NULL / not a number → 0
bool boolean(const Value &v); // true, 1, "t", "YES", "true"
// `text` split on `separator`, empty parts dropped ("a,b" → {a, b}).
QStringList list(const Value &v, QChar separator = u',');

} // namespace owl::catalog
