// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/catalog.h"
#include "owl/core/driver.h"

#include <QSet>

namespace owl::postgres {

class TypeRegistry;

// PostgreSQL catalog over pg_catalog (decision 163), PostgreSQL 13+. Tree:
//   database
//     schema → tables / views / materialized views / sequences / functions / procedures / types
//     extensions
//       table → columns / indexes / constraints / triggers;  view → columns;
//       materialized view → columns / indexes
// Only the connection's own database can be browsed: other databases answer Unsupported and
// need a connection of their own (driver-api.md §6.1). Objects carry their OID in ObjectRef::oid
// (function overloads share a name). Table DDL is assembled from the catalog (PostgreSQL has no
// pg_get_tabledef); everything else comes from the pg_get_*def functions.
class PgCatalog : public ICatalog
{
public:
    PgCatalog(IConnection &connection, const TypeRegistry &types)
        : m_conn(connection)
        , m_types(types)
    { }

    Result<QList<ObjectNode>> children(const ObjectPath &path) override;
    Result<TableDef> table(const ObjectPath &path) override;
    Result<QString> ddl(const ObjectPath &path) override;

    // Forgets per-connection facts (the current database); called when the connection reopens.
    void reset()
    {
        m_currentDatabase.clear();
        m_keywords.clear();
    }

private:
    Result<void> checkDatabase(const ObjectPath &path);
    Result<QString> relationOid(const ObjectPath &path); // the table / view the path is about
    Result<QList<ObjectNode>> schemaGroup(const QString &schema, const QString &group);
    Result<QList<ObjectNode>> relationGroup(const ObjectPath &path, const QString &oid,
                                            const QString &group);
    Result<QString> tableDdl(const QString &oid);
    Result<QString> typeDdl(const ObjectRef &type, const QString &schema);
    // Identifiers quoted like quote_ident(): only when they need it (the server's keywords).
    QString ident(const QString &name);
    QString qualified(const QString &schema, const QString &name)
    {
        return ident(schema) + u'.' + ident(name);
    }

    IConnection &m_conn;
    const TypeRegistry &m_types;
    QString m_currentDatabase;
    QSet<QString> m_keywords; // reserved / column-name keywords: quoted
};

} // namespace owl::postgres
