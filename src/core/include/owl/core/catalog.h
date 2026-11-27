// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/dberror.h"
#include "owl/core/result.h"
#include "owl/core/value.h"

#include <QAnyStringView>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Catalog: what a database contains, for the object tree, object tabs and DDL
// (docs/specs/driver-api.md §4). Every driver implements ICatalog on its connection; the UI renders
// the nodes generically and never branches on the engine.

namespace owl {

// Node kinds. Paths are made of these; engines use the subset they have.
namespace kind {
inline constexpr QLatin1StringView Database("database");
inline constexpr QLatin1StringView Schema("schema");
inline constexpr QLatin1StringView Group("group"); // a folder: name is one of group::*
inline constexpr QLatin1StringView Table("table");
inline constexpr QLatin1StringView View("view");
inline constexpr QLatin1StringView MaterializedView("matview");
inline constexpr QLatin1StringView Sequence("sequence");
inline constexpr QLatin1StringView Function("function");
inline constexpr QLatin1StringView Procedure("procedure");
inline constexpr QLatin1StringView Type("type");
inline constexpr QLatin1StringView Extension("extension");
inline constexpr QLatin1StringView Event("event");
inline constexpr QLatin1StringView Column("column");
inline constexpr QLatin1StringView Index("index");
inline constexpr QLatin1StringView Constraint("constraint");
inline constexpr QLatin1StringView Trigger("trigger");
} // namespace kind

// Group (folder) names: the ref.name of a kind::Group node.
namespace group {
inline constexpr QLatin1StringView Tables("tables");
inline constexpr QLatin1StringView Views("views");
inline constexpr QLatin1StringView MaterializedViews("matviews");
inline constexpr QLatin1StringView Sequences("sequences");
inline constexpr QLatin1StringView Functions("functions");
inline constexpr QLatin1StringView Procedures("procedures");
inline constexpr QLatin1StringView Types("types");
inline constexpr QLatin1StringView Extensions("extensions");
inline constexpr QLatin1StringView Events("events");
inline constexpr QLatin1StringView Columns("columns");
inline constexpr QLatin1StringView Indexes("indexes");
inline constexpr QLatin1StringView Constraints("constraints");
inline constexpr QLatin1StringView Triggers("triggers");
} // namespace group

struct ObjectRef
{
    QString kind; // kind::*
    QString name; // the object's name (functions: without arguments)
    QString oid; // engine-specific identity when names are not unique (PostgreSQL overloads)

    bool operator==(const ObjectRef &) const = default;
};

// Location of an object, from the top: [database:app, schema:public, group:tables, table:users].
struct ObjectPath
{
    QList<ObjectRef> parts;

    bool isRoot() const { return parts.isEmpty(); }
    qsizetype depth() const { return parts.size(); }
    const ObjectRef &last() const { return parts.last(); }
    ObjectPath parent() const { return {parts.first(qMax<qsizetype>(0, parts.size() - 1))}; }
    ObjectPath child(ObjectRef ref) const;
    // `this` equals `other` or lies below it.
    bool startsWith(const ObjectPath &other) const;
    // The name of the first part of this kind ("" if none): find(kind::Schema) → "public".
    QString find(QLatin1StringView kind) const;
    // "database:app/schema:public/table:users" — for logs and test messages.
    QString toString() const;

    bool operator==(const ObjectPath &) const = default;
};

size_t qHash(const ObjectRef &ref, size_t seed = 0);
size_t qHash(const ObjectPath &path, size_t seed = 0);

// One node of the object tree.
struct ObjectNode
{
    ObjectRef ref;
    QString displayName; // shown in the tree; "f(integer, text)" for functions
    QString icon; // Tabler icon name (decision 109)
    bool hasChildren = false;
    bool isSystem = false; // pg_catalog, information_schema, mysql, sqlite_sequence…
    // Extra facts for tooltips / badges: "comment", "rows" (estimate), "type", "engine",
    // "current" (the connection's own database)…
    QVariantMap badges;

    bool operator==(const ObjectNode &) const = default;
};

struct ColumnDef
{
    QString name;
    QString type; // as the engine formats it: "character varying(255)", "int unsigned"
    ValueKind kind = ValueKind::Unknown;
    bool nullable = true;
    QString defaultExpr; // SQL expression as the server reports it; empty = none
    bool autoIncrement = false; // serial / identity / AUTO_INCREMENT / SQLite rowid alias
    QString generatedExpr; // generated (computed) columns
    QString collation;
    QString comment;

    bool operator==(const ColumnDef &) const = default;
};

struct IndexDef
{
    QString name;
    QStringList columns; // column names or expressions, in index order
    bool unique = false;
    bool primary = false;
    QString method; // btree, hash, gin, BTREE, FULLTEXT…
    QString predicate; // partial index WHERE clause
    QString definition; // full CREATE INDEX text when the engine provides it

    bool operator==(const IndexDef &) const = default;
};

struct ForeignKeyDef
{
    QString name;
    QStringList columns;
    QString refSchema; // database for MySQL, schema for PostgreSQL, empty for SQLite
    QString refTable;
    QStringList refColumns;
    QString onUpdate; // "CASCADE", "SET NULL", "RESTRICT", "NO ACTION", "SET DEFAULT"
    QString onDelete;

    bool operator==(const ForeignKeyDef &) const = default;
};

struct ConstraintDef // unique and check constraints
{
    QString name;
    QStringList columns; // unique
    QString expression; // check

    bool operator==(const ConstraintDef &) const = default;
};

struct TriggerDef
{
    QString name;
    QString timing; // BEFORE, AFTER, INSTEAD OF
    QStringList events; // INSERT, UPDATE, DELETE, TRUNCATE
    QString definition; // full CREATE TRIGGER text when the engine provides it

    bool operator==(const TriggerDef &) const = default;
};

// The shared model for the table editor, grid editing (primary-key detection) and DDL
// generation (driver-api.md §4). Views and materialized views fill columns only.
struct TableDef
{
    QString schema; // PostgreSQL schema / MySQL database / SQLite database ("main")
    QString name;
    QString kind; // kind::Table, kind::View, kind::MaterializedView
    QString comment;
    QList<ColumnDef> columns;
    QString primaryKeyName;
    QStringList primaryKey;
    QList<ConstraintDef> uniques;
    QList<ConstraintDef> checks;
    QList<ForeignKeyDef> foreignKeys;
    QList<IndexDef> indexes; // all indexes, including the ones behind PK / UNIQUE
    QList<TriggerDef> triggers;
    // Engine extras: "rows" (estimate), "engine", "collation", "autoIncrement", "partitioned",
    // "withoutRowid", "strict"…
    QVariantMap options;

    const ColumnDef *column(QStringView columnName) const;
    bool operator==(const TableDef &) const = default;
};

// Introspection of one connection. Blocking, used on that connection's worker thread only,
// like IConnection (CatalogCache runs it there). Paths come from children() — callers do not
// build them by hand, except the root.
class ICatalog
{
public:
    virtual ~ICatalog() = default;
    // The nodes below `path` (root: the databases). Unsupported for a PostgreSQL database other
    // than the connection's own: browsing it needs a connection to it (driver-api.md §6.1).
    virtual Result<QList<ObjectNode>> children(const ObjectPath &path) = 0;
    // A table, view or materialized view.
    virtual Result<TableDef> table(const ObjectPath &path) = 0;
    // CREATE … text for the DDL tab: tables include their indexes, triggers and comments.
    virtual Result<QString> ddl(const ObjectPath &path) = 0;
    // View, routine and search APIs join with the editors (M4) and the command palette (M3).
};

// Standard icon of a node kind (and group / constraint flavour), shared by the drivers.
QString iconFor(QAnyStringView kind, QAnyStringView detail = {});
// A group node ("Tables") below a parent.
ObjectNode groupNode(QLatin1StringView group, bool hasChildren = true);

} // namespace owl
