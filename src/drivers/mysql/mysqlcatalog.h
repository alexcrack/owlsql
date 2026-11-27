// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/catalog.h"
#include "owl/core/driver.h"

namespace owl::mysql {

// MySQL / MariaDB catalog over information_schema and SHOW CREATE (decision 163). Tree:
//   database → tables / views / functions / procedures / triggers / events
//     table → columns / indexes / constraints / triggers;  view → columns
// All databases are browsable from one connection (no USE needed). Column defaults are reported
// as SQL expressions on both flavours (MySQL reports plain values; they are quoted here). CHECK
// constraints need MySQL 8.0.16+ or MariaDB.
class MySqlCatalog : public ICatalog
{
public:
    explicit MySqlCatalog(IConnection &connection)
        : m_conn(connection)
    { }

    Result<QList<ObjectNode>> children(const ObjectPath &path) override;
    Result<TableDef> table(const ObjectPath &path) override;
    Result<QString> ddl(const ObjectPath &path) override;

private:
    bool isMariaDb() const;
    Result<QString> showCreate(const QString &what, const QString &name, int column);

    IConnection &m_conn;
};

} // namespace owl::mysql
