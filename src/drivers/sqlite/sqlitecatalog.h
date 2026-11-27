// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/catalog.h"
#include "owl/core/driver.h"

namespace owl::sqlite {

// SQLite catalog over PRAGMA and sqlite_schema (decision 163). Tree:
//   database (main, attached; temp when it has objects)
//     tables / views / indexes / triggers
//       table → columns / indexes / constraints / triggers;  view → columns
// CHECK constraints and generated-column expressions are not reported by SQLite's pragmas; the
// DDL tab shows them in the original CREATE statement.
class SqliteCatalog : public ICatalog
{
public:
    explicit SqliteCatalog(IConnection &connection)
        : m_conn(connection)
    { }

    Result<QList<ObjectNode>> children(const ObjectPath &path) override;
    Result<TableDef> table(const ObjectPath &path) override;
    Result<QString> ddl(const ObjectPath &path) override;

private:
    IConnection &m_conn;
};

// Column kind from a declared type, by SQLite's affinity rules (shared with the driver).
ValueKind kindForDeclaredType(const QString &declared);

} // namespace owl::sqlite
