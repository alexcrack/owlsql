// SPDX-License-Identifier: GPL-3.0-or-later
#include "pgcatalog.h"

#include "owl/core/catalogsupport.h"
#include "pgdialect.h"
#include "pgvalues.h"

#include <QRegularExpression>

#include <array>

namespace owl::postgres {

using catalog::boolean;
using catalog::integer;
using catalog::query;
using catalog::Row;
using catalog::text;

namespace {

const PgDialect &dialect()
{
    static const PgDialect d;
    return d;
}

QString literal(const QString &s)
{
    return dialect().quoteLiteral(Value::text(s));
}

QStringList strings(const Value &v)
{
    QStringList out;
    for (const Value &e : v.toArray())
        out << text(e);
    return out;
}

ObjectNode node(QLatin1StringView kind, const QString &name, const QString &oid, bool hasChildren,
                QStringView iconDetail = {})
{
    ObjectNode n;
    n.ref = {.kind = kind, .name = name, .oid = oid};
    n.displayName = name;
    n.icon = iconFor(kind, iconDetail);
    n.hasChildren = hasChildren;
    return n;
}

void addComment(ObjectNode &n, const Value &comment)
{
    if (const QString c = text(comment); !c.isEmpty())
        n.badges.insert(QStringLiteral("comment"), c);
}

DbError badPath(const ObjectPath &path)
{
    DbError e;
    e.category = DbError::Category::Unsupported;
    e.message = QStringLiteral("No catalog entry for %1").arg(path.toString());
    return e;
}

QString fkAction(const QString &code)
{
    if (code == u"c")
        return QStringLiteral("CASCADE");
    if (code == u"n")
        return QStringLiteral("SET NULL");
    if (code == u"d")
        return QStringLiteral("SET DEFAULT");
    if (code == u"r")
        return QStringLiteral("RESTRICT");
    return QStringLiteral("NO ACTION");
}

// pg_trigger.tgtype bits (src/include/catalog/pg_trigger.h).
void decodeTriggerType(qint64 type, TriggerDef &t)
{
    t.timing = (type & 64) ? QStringLiteral("INSTEAD OF")
        : (type & 2)       ? QStringLiteral("BEFORE")
                           : QStringLiteral("AFTER");
    if (type & 4)
        t.events << QStringLiteral("INSERT");
    if (type & 16)
        t.events << QStringLiteral("UPDATE");
    if (type & 8)
        t.events << QStringLiteral("DELETE");
    if (type & 32)
        t.events << QStringLiteral("TRUNCATE");
}

// Relation kinds per group, and the node kind they become.
struct RelationGroup
{
    QLatin1StringView group;
    const char *relkinds;
    QLatin1StringView kind;
};
constexpr std::array relationGroups{
    RelationGroup{.group = group::Tables, .relkinds = "'r','p'", .kind = kind::Table},
    RelationGroup{.group = group::Views, .relkinds = "'v'", .kind = kind::View},
    RelationGroup{
        .group = group::MaterializedViews, .relkinds = "'m'", .kind = kind::MaterializedView},
    RelationGroup{.group = group::Sequences, .relkinds = "'S'", .kind = kind::Sequence},
};

} // namespace

QString PgCatalog::ident(const QString &name)
{
    if (m_keywords.isEmpty()) {
        auto rows = query(m_conn,
                          QStringLiteral("SELECT word FROM pg_catalog.pg_get_keywords() "
                                         "WHERE catcode <> 'U'"));
        if (rows) {
            for (const Row &r : *rows)
                m_keywords.insert(text(r.at(0)));
        }
        if (m_keywords.isEmpty())
            m_keywords.insert(QStringLiteral("select")); // not loaded: never ask again
    }
    static const QRegularExpression plain(QStringLiteral("^[a-z_][a-z0-9_$]*$"));
    if (plain.match(name).hasMatch() && !m_keywords.contains(name))
        return name;
    return dialect().quoteIdentifier(name);
}

Result<void> PgCatalog::checkDatabase(const ObjectPath &path)
{
    if (m_currentDatabase.isEmpty()) {
        auto r = query(m_conn, QStringLiteral("SELECT current_database()"));
        if (!r)
            return fail(r.error());
        m_currentDatabase = text(r->value(0).value(0));
    }
    const QString db = path.find(kind::Database);
    if (!db.isEmpty() && db != m_currentDatabase) {
        DbError e;
        e.category = DbError::Category::Unsupported;
        e.message = QStringLiteral("Database \"%1\" needs its own connection (this one is "
                                   "connected to \"%2\")")
                        .arg(db, m_currentDatabase);
        return fail(e);
    }
    return {};
}

Result<QList<ObjectNode>> PgCatalog::children(const ObjectPath &path)
{
    if (Result<void> ok = checkDatabase(path); !ok)
        return fail(ok.error());
    QList<ObjectNode> out;

    if (path.isRoot()) {
        auto rows
            = query(m_conn,
                    QStringLiteral("SELECT datname, oid, shobj_description(oid, 'pg_database') "
                                   "FROM pg_catalog.pg_database "
                                   "WHERE NOT datistemplate AND datallowconn ORDER BY datname"));
        if (!rows)
            return fail(rows.error());
        for (const Row &r : *rows) {
            ObjectNode n = node(kind::Database, text(r.at(0)), text(r.at(1)), true);
            addComment(n, r.at(2));
            if (n.ref.name == m_currentDatabase)
                n.badges.insert(QStringLiteral("current"), true);
            out.append(n);
        }
        return out;
    }

    const ObjectRef &last = path.last();
    if (path.depth() == 1) { // a database: its schemas, then the extensions group
        auto rows = query(
            m_conn,
            QStringLiteral("SELECT nspname, oid, obj_description(oid, 'pg_namespace') "
                           "FROM pg_catalog.pg_namespace "
                           "WHERE nspname !~ '^pg_toast' AND nspname !~ '^pg_temp_' "
                           "ORDER BY nspname IN ('pg_catalog', 'information_schema'), nspname"));
        if (!rows)
            return fail(rows.error());
        for (const Row &r : *rows) {
            ObjectNode n = node(kind::Schema, text(r.at(0)), text(r.at(1)), true);
            n.isSystem = n.ref.name == u"pg_catalog" || n.ref.name == u"information_schema";
            addComment(n, r.at(2));
            out.append(n);
        }
        out.append(groupNode(group::Extensions));
        return out;
    }

    if (path.depth() == 2 && last.kind == kind::Group && last.name == group::Extensions) {
        auto rows = query(m_conn,
                          QStringLiteral("SELECT e.extname, e.oid, e.extversion, n.nspname "
                                         "FROM pg_catalog.pg_extension e "
                                         "JOIN pg_catalog.pg_namespace n ON n.oid = e.extnamespace "
                                         "ORDER BY e.extname"));
        if (!rows)
            return fail(rows.error());
        for (const Row &r : *rows) {
            ObjectNode n = node(kind::Extension, text(r.at(0)), text(r.at(1)), false);
            n.badges.insert(QStringLiteral("version"), text(r.at(2)));
            n.badges.insert(QStringLiteral("schema"), text(r.at(3)));
            out.append(n);
        }
        return out;
    }

    const QString schema = path.find(kind::Schema);
    if (path.depth() == 2 && last.kind == kind::Schema) {
        return QList<ObjectNode>{groupNode(group::Tables),
                                 groupNode(group::Views),
                                 groupNode(group::MaterializedViews),
                                 groupNode(group::Sequences),
                                 groupNode(group::Functions),
                                 groupNode(group::Procedures),
                                 groupNode(group::Types)};
    }
    if (path.depth() == 3 && last.kind == kind::Group)
        return schemaGroup(schema, last.name);

    // Below a relation.
    Result<QString> oid = relationOid(path);
    if (!oid)
        return fail(oid.error());
    if (last.kind == kind::Table)
        return QList<ObjectNode>{groupNode(group::Columns), groupNode(group::Indexes),
                                 groupNode(group::Constraints), groupNode(group::Triggers)};
    if (last.kind == kind::View)
        return QList<ObjectNode>{groupNode(group::Columns)};
    if (last.kind == kind::MaterializedView)
        return QList<ObjectNode>{groupNode(group::Columns), groupNode(group::Indexes)};
    if (last.kind == kind::Group)
        return relationGroup(path, *oid, last.name);
    return out; // leaves
}

Result<QList<ObjectNode>> PgCatalog::schemaGroup(const QString &schema, const QString &group)
{
    QList<ObjectNode> out;
    const QList<Value> params{Value::text(schema)};
    for (const RelationGroup &g : relationGroups) {
        if (group != g.group)
            continue;
        auto rows = query(m_conn,
                          QStringLiteral("SELECT c.relname, c.oid, c.reltuples::bigint, "
                                         "obj_description(c.oid, 'pg_class'), c.relkind "
                                         "FROM pg_catalog.pg_class c "
                                         "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
                                         "WHERE n.nspname = $1 AND c.relkind IN (%1) "
                                         "AND NOT c.relispartition ORDER BY c.relname")
                              .arg(QLatin1String(g.relkinds)),
                          params);
        if (!rows)
            return fail(rows.error());
        for (const Row &r : *rows) {
            ObjectNode n = node(g.kind, text(r.at(0)), text(r.at(1)), g.kind != kind::Sequence);
            if (const qint64 rowsEstimate = integer(r.at(2));
                rowsEstimate >= 0 && g.kind != kind::Sequence && g.kind != kind::View)
                n.badges.insert(QStringLiteral("rows"), rowsEstimate);
            if (text(r.at(4)) == u"p")
                n.badges.insert(QStringLiteral("partitioned"), true);
            addComment(n, r.at(3));
            out.append(n);
        }
        return out;
    }
    if (group == group::Functions || group == group::Procedures) {
        const bool procedures = group == group::Procedures;
        auto rows = query(
            m_conn,
            QStringLiteral("SELECT p.proname, p.oid, pg_get_function_identity_arguments(p.oid), "
                           "obj_description(p.oid, 'pg_proc'), p.prokind "
                           "FROM pg_catalog.pg_proc p "
                           "JOIN pg_catalog.pg_namespace n ON n.oid = p.pronamespace "
                           "WHERE n.nspname = $1 AND p.prokind %1 ORDER BY 1, 3")
                .arg(procedures ? QStringLiteral("= 'p'") : QStringLiteral("IN ('f', 'a', 'w')")),
            params);
        if (!rows)
            return fail(rows.error());
        for (const Row &r : *rows) {
            ObjectNode n = node(procedures ? kind::Procedure : kind::Function, text(r.at(0)),
                                text(r.at(1)), false);
            n.displayName = QStringLiteral("%1(%2)").arg(n.ref.name, text(r.at(2)));
            if (text(r.at(4)) == u"a")
                n.badges.insert(QStringLiteral("aggregate"), true);
            addComment(n, r.at(3));
            out.append(n);
        }
        return out;
    }
    if (group == group::Types) {
        // Enums, domains, ranges and standalone composite types (not arrays, not table rows).
        auto rows = query(
            m_conn,
            QStringLiteral("SELECT t.typname, t.oid, t.typtype, obj_description(t.oid, 'pg_type') "
                           "FROM pg_catalog.pg_type t "
                           "JOIN pg_catalog.pg_namespace n ON n.oid = t.typnamespace "
                           "LEFT JOIN pg_catalog.pg_class c ON c.oid = t.typrelid "
                           "WHERE n.nspname = $1 AND (t.typtype IN ('e', 'd', 'r') "
                           "OR (t.typtype = 'c' AND c.relkind = 'c')) ORDER BY t.typname"),
            params);
        if (!rows)
            return fail(rows.error());
        static const QHash<QString, QString> flavours{
            {QStringLiteral("e"), QStringLiteral("enum")},
            {QStringLiteral("d"), QStringLiteral("domain")},
            {QStringLiteral("r"), QStringLiteral("range")},
            {QStringLiteral("c"), QStringLiteral("composite")}};
        for (const Row &r : *rows) {
            ObjectNode n = node(kind::Type, text(r.at(0)), text(r.at(1)), false);
            n.badges.insert(QStringLiteral("type"), flavours.value(text(r.at(2))));
            addComment(n, r.at(3));
            out.append(n);
        }
        return out;
    }
    DbError e;
    e.category = DbError::Category::Unsupported;
    e.message = QStringLiteral("Unknown group \"%1\"").arg(group);
    return fail(e);
}

Result<QList<ObjectNode>> PgCatalog::relationGroup(const ObjectPath &path, const QString &oid,
                                                   const QString &group)
{
    QList<ObjectNode> out;
    const QList<Value> params{Value::text(oid)};
    if (group == group::Columns) {
        Result<TableDef> def = table(path);
        if (!def)
            return fail(def.error());
        for (const ColumnDef &c : def->columns) {
            const bool pk = def->primaryKey.contains(c.name);
            ObjectNode n = node(kind::Column, c.name, {}, false, pk ? u"primary" : u"");
            n.badges.insert(QStringLiteral("type"), c.type);
            n.badges.insert(QStringLiteral("nullable"), c.nullable);
            if (!c.comment.isEmpty())
                n.badges.insert(QStringLiteral("comment"), c.comment);
            out.append(n);
        }
        return out;
    }
    if (group == group::Indexes) {
        auto rows = query(m_conn,
                          QStringLiteral("SELECT c.relname, c.oid, i.indisunique, i.indisprimary "
                                         "FROM pg_catalog.pg_index i "
                                         "JOIN pg_catalog.pg_class c ON c.oid = i.indexrelid "
                                         "WHERE i.indrelid = $1::oid ORDER BY c.relname"),
                          params);
        if (!rows)
            return fail(rows.error());
        for (const Row &r : *rows) {
            ObjectNode n = node(kind::Index, text(r.at(0)), text(r.at(1)), false);
            n.badges.insert(QStringLiteral("unique"), boolean(r.at(2)));
            n.badges.insert(QStringLiteral("primary"), boolean(r.at(3)));
            out.append(n);
        }
        return out;
    }
    if (group == group::Constraints) {
        auto rows
            = query(m_conn,
                    QStringLiteral("SELECT conname, oid, contype, pg_get_constraintdef(oid, true) "
                                   "FROM pg_catalog.pg_constraint "
                                   "WHERE conrelid = $1::oid AND contype <> 'n' "
                                   "ORDER BY position(contype IN 'pufcx'), conname"),
                    params);
        if (!rows)
            return fail(rows.error());
        static const QHash<QString, QString> flavours{
            {QStringLiteral("p"), QStringLiteral("primary")},
            {QStringLiteral("u"), QStringLiteral("unique")},
            {QStringLiteral("f"), QStringLiteral("foreign")},
            {QStringLiteral("c"), QStringLiteral("check")},
            {QStringLiteral("x"), QStringLiteral("exclusion")}};
        for (const Row &r : *rows) {
            const QString flavour = flavours.value(text(r.at(2)));
            ObjectNode n = node(kind::Constraint, text(r.at(0)), text(r.at(1)), false, flavour);
            n.badges.insert(QStringLiteral("type"), flavour);
            n.badges.insert(QStringLiteral("definition"), text(r.at(3)));
            out.append(n);
        }
        return out;
    }
    if (group == group::Triggers) {
        auto rows
            = query(m_conn,
                    QStringLiteral("SELECT tgname, oid, tgenabled FROM pg_catalog.pg_trigger "
                                   "WHERE tgrelid = $1::oid AND NOT tgisinternal ORDER BY tgname"),
                    params);
        if (!rows)
            return fail(rows.error());
        for (const Row &r : *rows) {
            ObjectNode n = node(kind::Trigger, text(r.at(0)), text(r.at(1)), false);
            n.badges.insert(QStringLiteral("enabled"), text(r.at(2)) != u"D");
            out.append(n);
        }
        return out;
    }
    return fail(badPath(path));
}

Result<QString> PgCatalog::relationOid(const ObjectPath &path)
{
    for (qsizetype i = path.parts.size() - 1; i >= 0; --i) {
        const ObjectRef &r = path.parts.at(i);
        if (r.kind != kind::Table && r.kind != kind::View && r.kind != kind::MaterializedView)
            continue;
        if (!r.oid.isEmpty())
            return r.oid;
        auto rows = query(m_conn,
                          QStringLiteral("SELECT c.oid FROM pg_catalog.pg_class c "
                                         "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
                                         "WHERE n.nspname = $1 AND c.relname = $2"),
                          {Value::text(path.find(kind::Schema)), Value::text(r.name)});
        if (!rows)
            return fail(rows.error());
        if (rows->isEmpty())
            break;
        return text(rows->first().at(0));
    }
    DbError e;
    e.message = QStringLiteral("No such table or view: %1").arg(path.toString());
    return fail(e);
}

Result<TableDef> PgCatalog::table(const ObjectPath &path)
{
    if (Result<void> ok = checkDatabase(path); !ok)
        return fail(ok.error());
    Result<QString> oid = relationOid(path);
    if (!oid)
        return fail(oid.error());
    const QList<Value> params{Value::text(*oid)};

    auto head = query(m_conn,
                      QStringLiteral("SELECT c.relname, n.nspname, c.relkind, "
                                     "obj_description(c.oid, 'pg_class'), c.reltuples::bigint, "
                                     "c.relpersistence "
                                     "FROM pg_catalog.pg_class c "
                                     "JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace "
                                     "WHERE c.oid = $1::oid"),
                      params);
    if (!head)
        return fail(head.error());
    if (head->isEmpty()) {
        DbError e;
        e.message = QStringLiteral("No such table or view: %1").arg(path.toString());
        return fail(e);
    }
    const Row &h = head->first();
    TableDef t;
    t.name = text(h.at(0));
    t.schema = text(h.at(1));
    const QString relkind = text(h.at(2));
    t.kind = relkind == u"v" ? QString(kind::View)
        : relkind == u"m"    ? QString(kind::MaterializedView)
                             : QString(kind::Table);
    t.comment = text(h.at(3));
    if (const qint64 rows = integer(h.at(4)); rows >= 0 && t.kind != kind::View)
        t.options.insert(QStringLiteral("rows"), rows);
    if (relkind == u"p")
        t.options.insert(QStringLiteral("partitioned"), true);
    if (text(h.at(5)) == u"u")
        t.options.insert(QStringLiteral("unlogged"), true);

    auto cols = query(
        m_conn,
        QStringLiteral("SELECT a.attname, format_type(a.atttypid, a.atttypmod), a.atttypid, "
                       "a.attnotnull, pg_get_expr(d.adbin, d.adrelid), a.attidentity, "
                       "a.attgenerated, co.collname, col_description(a.attrelid, a.attnum) "
                       "FROM pg_catalog.pg_attribute a "
                       "LEFT JOIN pg_catalog.pg_attrdef d "
                       "ON d.adrelid = a.attrelid AND d.adnum = a.attnum "
                       "LEFT JOIN pg_catalog.pg_type ty ON ty.oid = a.atttypid "
                       "LEFT JOIN pg_catalog.pg_collation co "
                       "ON co.oid = a.attcollation AND a.attcollation <> ty.typcollation "
                       "WHERE a.attrelid = $1::oid AND a.attnum > 0 AND NOT a.attisdropped "
                       "ORDER BY a.attnum"),
        params);
    if (!cols)
        return fail(cols.error());
    for (const Row &r : *cols) {
        ColumnDef c;
        c.name = text(r.at(0));
        c.type = text(r.at(1));
        c.kind = m_types.kind(static_cast<quint32>(integer(r.at(2))));
        c.nullable = !boolean(r.at(3));
        c.defaultExpr = text(r.at(4));
        const QString identity = text(r.at(5));
        if (text(r.at(6)) == u"s") { // generated column: the "default" is its expression
            c.generatedExpr = c.defaultExpr;
            c.defaultExpr.clear();
        }
        c.autoIncrement = !identity.isEmpty() || c.defaultExpr.startsWith(u"nextval(");
        c.collation = text(r.at(7));
        c.comment = text(r.at(8));
        t.columns.append(c);
    }
    if (t.kind == kind::View)
        return t;

    // Constraints, with column names resolved in key order (NOT NULL constraints of PostgreSQL 18
    // are already in ColumnDef::nullable).
    const QString keyNames
        = QStringLiteral("ARRAY(SELECT a.attname FROM unnest(%1) WITH ORDINALITY k(attnum, ord) "
                         "JOIN pg_catalog.pg_attribute a ON a.attrelid = %2 AND a.attnum = "
                         "k.attnum ORDER BY k.ord)");
    auto cons
        = query(m_conn,
                QStringLiteral("SELECT c.conname, c.contype, %1, %2, rn.nspname, rc.relname, "
                               "c.confupdtype, c.confdeltype, pg_get_expr(c.conbin, c.conrelid) "
                               "FROM pg_catalog.pg_constraint c "
                               "LEFT JOIN pg_catalog.pg_class rc ON rc.oid = c.confrelid "
                               "LEFT JOIN pg_catalog.pg_namespace rn ON rn.oid = rc.relnamespace "
                               "WHERE c.conrelid = $1::oid AND c.contype IN ('p', 'u', 'f', 'c') "
                               "ORDER BY c.conname")
                    .arg(keyNames.arg(QStringLiteral("c.conkey"), QStringLiteral("c.conrelid")),
                         keyNames.arg(QStringLiteral("c.confkey"), QStringLiteral("c.confrelid"))),
                params);
    if (!cons)
        return fail(cons.error());
    for (const Row &r : *cons) {
        const QString type = text(r.at(1));
        const QString name = text(r.at(0));
        const QStringList columns = strings(r.at(2));
        if (type == u"p") {
            t.primaryKeyName = name;
            t.primaryKey = columns;
        } else if (type == u"u") {
            t.uniques.append({.name = name, .columns = columns, .expression = {}});
        } else if (type == u"c") {
            t.checks.append({.name = name, .columns = {}, .expression = text(r.at(8))});
        } else {
            t.foreignKeys.append({.name = name,
                                  .columns = columns,
                                  .refSchema = text(r.at(4)),
                                  .refTable = text(r.at(5)),
                                  .refColumns = strings(r.at(3)),
                                  .onUpdate = fkAction(text(r.at(6))),
                                  .onDelete = fkAction(text(r.at(7)))});
        }
    }

    auto idx
        = query(m_conn,
                QStringLiteral("SELECT ic.relname, i.indisunique, i.indisprimary, am.amname, "
                               "pg_get_indexdef(i.indexrelid), pg_get_expr(i.indpred, i.indrelid), "
                               "ARRAY(SELECT pg_get_indexdef(i.indexrelid, k, true) "
                               "FROM generate_series(1, i.indnkeyatts) k ORDER BY k) "
                               "FROM pg_catalog.pg_index i "
                               "JOIN pg_catalog.pg_class ic ON ic.oid = i.indexrelid "
                               "JOIN pg_catalog.pg_am am ON am.oid = ic.relam "
                               "WHERE i.indrelid = $1::oid ORDER BY ic.relname"),
                params);
    if (!idx)
        return fail(idx.error());
    for (const Row &r : *idx) {
        t.indexes.append({.name = text(r.at(0)),
                          .columns = strings(r.at(6)),
                          .unique = boolean(r.at(1)),
                          .primary = boolean(r.at(2)),
                          .method = text(r.at(3)),
                          .predicate = text(r.at(5)),
                          .definition = text(r.at(4))});
    }

    auto trg = query(m_conn,
                     QStringLiteral("SELECT tgname, pg_get_triggerdef(oid, true), tgtype "
                                    "FROM pg_catalog.pg_trigger "
                                    "WHERE tgrelid = $1::oid AND NOT tgisinternal ORDER BY tgname"),
                     params);
    if (!trg)
        return fail(trg.error());
    for (const Row &r : *trg) {
        TriggerDef tr;
        tr.name = text(r.at(0));
        tr.definition = text(r.at(1));
        decodeTriggerType(integer(r.at(2)), tr);
        t.triggers.append(tr);
    }
    return t;
}

Result<QString> PgCatalog::tableDdl(const QString &oid)
{
    ObjectPath byOid;
    byOid.parts.append({.kind = kind::Table, .name = {}, .oid = oid});
    Result<TableDef> t = table(byOid);
    if (!t)
        return fail(t.error());
    const QString name = qualified(t->schema, t->name);
    const QList<Value> params{Value::text(oid)};

    QStringList lines;
    for (const ColumnDef &c : t->columns) {
        QString line = QStringLiteral("    %1 %2").arg(ident(c.name), c.type);
        if (!c.collation.isEmpty())
            line += QStringLiteral(" COLLATE ") + ident(c.collation);
        if (!c.generatedExpr.isEmpty())
            line += QStringLiteral(" GENERATED ALWAYS AS (%1) STORED").arg(c.generatedExpr);
        else if (!c.defaultExpr.isEmpty())
            line += QStringLiteral(" DEFAULT ") + c.defaultExpr;
        if (!c.nullable)
            line += QStringLiteral(" NOT NULL");
        lines << line;
    }
    auto ident_ = query(m_conn,
                        QStringLiteral("SELECT attname, attidentity FROM pg_catalog.pg_attribute "
                                       "WHERE attrelid = $1::oid AND attidentity <> ''"),
                        params);
    if (!ident_)
        return fail(ident_.error());
    for (const Row &r : *ident_) {
        const QString column = QStringLiteral("    %1 ").arg(ident(text(r.at(0))));
        for (QString &line : lines) {
            if (line.startsWith(column)) {
                line += text(r.at(1)) == u"a" ? QStringLiteral(" GENERATED ALWAYS AS IDENTITY")
                                              : QStringLiteral(" GENERATED BY DEFAULT AS IDENTITY");
            }
        }
    }
    auto cons = query(m_conn,
                      QStringLiteral("SELECT conname, pg_get_constraintdef(oid, true) "
                                     "FROM pg_catalog.pg_constraint "
                                     "WHERE conrelid = $1::oid AND contype <> 'n' "
                                     "ORDER BY position(contype IN 'pucxf'), conname"),
                      params);
    if (!cons)
        return fail(cons.error());
    for (const Row &r : *cons)
        lines << QStringLiteral("    CONSTRAINT %1 %2").arg(ident(text(r.at(0))), text(r.at(1)));

    const bool unlogged = t->options.value(QStringLiteral("unlogged")).toBool();
    QString out = QStringLiteral("CREATE %1TABLE %2 (\n%3\n)")
                      .arg(unlogged ? QStringLiteral("UNLOGGED ") : QString(), name,
                           lines.join(QStringLiteral(",\n")));
    if (t->options.value(QStringLiteral("partitioned")).toBool()) {
        auto key = query(m_conn, QStringLiteral("SELECT pg_get_partkeydef($1::oid)"), params);
        if (key && !key->isEmpty())
            out += QStringLiteral(" PARTITION BY ") + text(key->first().at(0));
    }
    out += QStringLiteral(";\n");

    if (!t->comment.isEmpty())
        out += QStringLiteral("\nCOMMENT ON TABLE %1 IS %2;\n").arg(name, literal(t->comment));
    for (const ColumnDef &c : t->columns) {
        if (!c.comment.isEmpty())
            out += QStringLiteral("COMMENT ON COLUMN %1.%2 IS %3;\n")
                       .arg(name, ident(c.name), literal(c.comment));
    }
    // Indexes that do not back a constraint (those are part of the CREATE TABLE).
    auto idx
        = query(m_conn,
                QStringLiteral("SELECT pg_get_indexdef(i.indexrelid) FROM pg_catalog.pg_index i "
                               "WHERE i.indrelid = $1::oid AND NOT EXISTS (SELECT 1 FROM "
                               "pg_catalog.pg_constraint c WHERE c.conindid = i.indexrelid "
                               "AND c.conrelid = i.indrelid) ORDER BY 1"),
                params);
    if (!idx)
        return fail(idx.error());
    if (!idx->isEmpty())
        out += u'\n';
    for (const Row &r : *idx)
        out += text(r.at(0)) + QStringLiteral(";\n");
    if (!t->triggers.isEmpty())
        out += u'\n';
    for (const TriggerDef &tr : t->triggers)
        out += tr.definition + QStringLiteral(";\n");
    return out;
}

Result<QString> PgCatalog::typeDdl(const ObjectRef &type, const QString &schema)
{
    const QList<Value> params{Value::text(type.oid)};
    auto head
        = query(m_conn,
                QStringLiteral("SELECT typtype, format_type(typbasetype, typtypmod), typnotnull, "
                               "typdefault FROM pg_catalog.pg_type WHERE oid = $1::oid"),
                params);
    if (!head)
        return fail(head.error());
    if (head->isEmpty())
        return fail(DbError::Category::Other, QStringLiteral("No such type: %1").arg(type.name));
    const Row &h = head->first();
    const QString name = qualified(schema, type.name);
    const QString typtype = text(h.at(0));
    if (typtype == u"e") {
        auto labels = query(m_conn,
                            QStringLiteral("SELECT enumlabel FROM pg_catalog.pg_enum "
                                           "WHERE enumtypid = $1::oid ORDER BY enumsortorder"),
                            params);
        if (!labels)
            return fail(labels.error());
        QStringList quoted;
        for (const Row &r : *labels)
            quoted << literal(text(r.at(0)));
        return QStringLiteral("CREATE TYPE %1 AS ENUM (%2);\n").arg(name, quoted.join(u", "));
    }
    if (typtype == u"d") {
        QString out = QStringLiteral("CREATE DOMAIN %1 AS %2").arg(name, text(h.at(1)));
        if (const QString def = text(h.at(3)); !def.isEmpty())
            out += QStringLiteral(" DEFAULT ") + def;
        if (boolean(h.at(2)))
            out += QStringLiteral(" NOT NULL");
        auto cons
            = query(m_conn,
                    QStringLiteral("SELECT conname, pg_get_constraintdef(oid, true) "
                                   "FROM pg_catalog.pg_constraint "
                                   "WHERE contypid = $1::oid AND contype = 'c' ORDER BY conname"),
                    params);
        if (!cons)
            return fail(cons.error());
        for (const Row &r : *cons)
            out += QStringLiteral("\n    CONSTRAINT %1 %2")
                       .arg(ident(text(r.at(0))), text(r.at(1)));
        return out + QStringLiteral(";\n");
    }
    if (typtype == u"r") {
        auto sub
            = query(m_conn,
                    QStringLiteral("SELECT format_type(rngsubtype, NULL) FROM pg_catalog.pg_range "
                                   "WHERE rngtypid = $1::oid"),
                    params);
        if (!sub)
            return fail(sub.error());
        if (sub->isEmpty())
            return fail(DbError::Category::Other,
                        QStringLiteral("No such range: %1").arg(type.name));
        return QStringLiteral("CREATE TYPE %1 AS RANGE (SUBTYPE = %2);\n")
            .arg(name, text(sub->first().at(0)));
    }
    auto attrs
        = query(m_conn,
                QStringLiteral("SELECT a.attname, format_type(a.atttypid, a.atttypmod) "
                               "FROM pg_catalog.pg_type t "
                               "JOIN pg_catalog.pg_attribute a ON a.attrelid = t.typrelid "
                               "WHERE t.oid = $1::oid AND a.attnum > 0 AND NOT a.attisdropped "
                               "ORDER BY a.attnum"),
                params);
    if (!attrs)
        return fail(attrs.error());
    QStringList fields;
    for (const Row &r : *attrs)
        fields << QStringLiteral("    %1 %2").arg(ident(text(r.at(0))), text(r.at(1)));
    return QStringLiteral("CREATE TYPE %1 AS (\n%2\n);\n")
        .arg(name, fields.join(QStringLiteral(",\n")));
}

Result<QString> PgCatalog::ddl(const ObjectPath &path)
{
    if (Result<void> ok = checkDatabase(path); !ok)
        return fail(ok.error());
    if (path.isRoot())
        return fail(badPath(path));
    const ObjectRef &last = path.last();
    const QString schema = path.find(kind::Schema);
    // One value from one query, as DDL text.
    const auto single = [this](const QString &sql, const QString &oid) -> Result<QString> {
        auto rows = query(m_conn, sql, {Value::text(oid)});
        if (!rows)
            return fail(rows.error());
        if (rows->isEmpty())
            return fail(DbError::Category::Other, QStringLiteral("Object not found"));
        return text(rows->first().at(0));
    };

    if (last.kind == kind::Table) {
        Result<QString> oid = relationOid(path);
        if (!oid)
            return fail(oid.error());
        return tableDdl(*oid);
    }
    if (last.kind == kind::View || last.kind == kind::MaterializedView) {
        Result<QString> oid = relationOid(path);
        if (!oid)
            return fail(oid.error());
        Result<QString> def = single(QStringLiteral("SELECT pg_get_viewdef($1::oid, true)"), *oid);
        if (!def)
            return def;
        const QString name = qualified(schema, last.name);
        if (last.kind == kind::View)
            return QStringLiteral("CREATE OR REPLACE VIEW %1 AS\n%2\n").arg(name, *def);
        QString out = QStringLiteral("CREATE MATERIALIZED VIEW %1 AS\n%2\nWITH DATA;\n")
                          .arg(name, def->chopped(1));
        auto idx = query(m_conn,
                         QStringLiteral("SELECT pg_get_indexdef(indexrelid) "
                                        "FROM pg_catalog.pg_index WHERE indrelid = $1::oid "
                                        "ORDER BY 1"),
                         {Value::text(*oid)});
        if (idx) {
            for (const Row &r : *idx)
                out += text(r.at(0)) + QStringLiteral(";\n");
        }
        return out;
    }
    if (last.kind == kind::Function || last.kind == kind::Procedure)
        return single(QStringLiteral("SELECT pg_get_functiondef($1::oid)"), last.oid);
    if (last.kind == kind::Index)
        return single(QStringLiteral("SELECT pg_get_indexdef($1::oid) || ';'"), last.oid);
    if (last.kind == kind::Trigger)
        return single(QStringLiteral("SELECT pg_get_triggerdef($1::oid, true) || ';'"), last.oid);
    if (last.kind == kind::Constraint) {
        return single(
            QStringLiteral("SELECT format('ALTER TABLE %s ADD CONSTRAINT %I %s;', "
                           "conrelid::regclass, conname, pg_get_constraintdef(oid, true)) "
                           "FROM pg_catalog.pg_constraint WHERE oid = $1::oid"),
            last.oid);
    }
    if (last.kind == kind::Sequence) {
        return single(
            QStringLiteral("SELECT format('CREATE SEQUENCE %s AS %s INCREMENT BY %s "
                           "MINVALUE %s MAXVALUE %s START WITH %s CACHE %s%s;', "
                           "seqrelid::regclass, format_type(seqtypid, NULL), seqincrement, "
                           "seqmin, seqmax, seqstart, seqcache, "
                           "CASE WHEN seqcycle THEN ' CYCLE' ELSE '' END) "
                           "FROM pg_catalog.pg_sequence WHERE seqrelid = $1::oid"),
            last.oid);
    }
    if (last.kind == kind::Type)
        return typeDdl(last, schema);
    if (last.kind == kind::Schema) {
        return single(QStringLiteral("SELECT format('CREATE SCHEMA %I AUTHORIZATION %I;', nspname, "
                                     "pg_get_userbyid(nspowner)) "
                                     "FROM pg_catalog.pg_namespace WHERE oid = $1::oid"),
                      last.oid);
    }
    if (last.kind == kind::Extension) {
        return single(
            QStringLiteral("SELECT format('CREATE EXTENSION %I WITH SCHEMA %I VERSION %L;', "
                           "e.extname, n.nspname, e.extversion) "
                           "FROM pg_catalog.pg_extension e "
                           "JOIN pg_catalog.pg_namespace n ON n.oid = e.extnamespace "
                           "WHERE e.oid = $1::oid"),
            last.oid);
    }
    if (last.kind == kind::Database) {
        return single(QStringLiteral("SELECT format('CREATE DATABASE %I WITH OWNER = %I "
                                     "ENCODING = %L TEMPLATE = template0;', datname, "
                                     "pg_get_userbyid(datdba), pg_encoding_to_char(encoding)) "
                                     "FROM pg_catalog.pg_database WHERE oid = $1::oid"),
                      last.oid);
    }
    return fail(badPath(path));
}

} // namespace owl::postgres
