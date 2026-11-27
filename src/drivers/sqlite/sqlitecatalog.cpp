// SPDX-License-Identifier: GPL-3.0-or-later
#include "sqlitecatalog.h"

#include "owl/core/catalogsupport.h"
#include "sqlitedialect.h"

#include <QRegularExpression>

namespace owl::sqlite {

using catalog::boolean;
using catalog::integer;
using catalog::query;
using catalog::text;

namespace {

const SqliteDialect &dialect()
{
    static const SqliteDialect d;
    return d;
}

QString ident(const QString &name)
{
    return dialect().quoteIdentifier(name);
}

QString literal(const QString &s)
{
    return dialect().quoteLiteral(Value::text(s));
}

ObjectNode node(QLatin1StringView kind, const QString &name, bool hasChildren,
                QStringView iconDetail = {})
{
    ObjectNode n;
    n.ref = {.kind = kind, .name = name, .oid = {}};
    n.displayName = name;
    n.icon = iconFor(kind, iconDetail);
    n.hasChildren = hasChildren;
    return n;
}

// The table or view a path points at (or lies below).
struct Target
{
    QString database;
    QString name;
    QString kind; // kind::Table / kind::View
};

std::optional<Target> targetOf(const ObjectPath &path)
{
    Target t;
    t.database = path.find(kind::Database);
    for (const ObjectRef &r : path.parts) {
        if (r.kind == kind::Table || r.kind == kind::View) {
            t.name = r.name;
            t.kind = r.kind;
        }
    }
    if (t.database.isEmpty() || t.name.isEmpty())
        return std::nullopt;
    return t;
}

DbError badPath(const ObjectPath &path)
{
    DbError e;
    e.category = DbError::Category::Unsupported;
    e.message = QStringLiteral("No catalog entry for %1").arg(path.toString());
    return e;
}

// "BEFORE UPDATE OF x ON t" → timing and events from a CREATE TRIGGER statement.
void parseTrigger(const QString &sql, TriggerDef &t)
{
    static const QRegularExpression re(
        QStringLiteral("\\b(BEFORE|AFTER|INSTEAD\\s+OF)\\s+(INSERT|UPDATE|DELETE)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(sql);
    t.timing = m.hasMatch() ? m.captured(1).toUpper().simplified() : QStringLiteral("BEFORE");
    if (m.hasMatch())
        t.events = {m.captured(2).toUpper()};
}

} // namespace

ValueKind kindForDeclaredType(const QString &declared)
{
    // SQLite type affinity rules (https://sqlite.org/datatype3.html §3.1).
    const QString t = declared.toUpper();
    if (t.isEmpty())
        return ValueKind::Unknown;
    if (t.contains(QLatin1String("BOOL")))
        return ValueKind::Bool;
    if (t.contains(QLatin1String("JSON")))
        return ValueKind::Json;
    if (t.contains(QLatin1String("INT")))
        return ValueKind::Int;
    if (t.contains(QLatin1String("CHAR")) || t.contains(QLatin1String("CLOB"))
        || t.contains(QLatin1String("TEXT")))
        return ValueKind::Text;
    if (t.contains(QLatin1String("BLOB")))
        return ValueKind::Bytes;
    if (t.contains(QLatin1String("REAL")) || t.contains(QLatin1String("FLOA"))
        || t.contains(QLatin1String("DOUB")))
        return ValueKind::Double;
    return ValueKind::Decimal; // NUMERIC affinity
}

Result<QList<ObjectNode>> SqliteCatalog::children(const ObjectPath &path)
{
    QList<ObjectNode> out;
    if (path.isRoot()) {
        auto dbs = query(m_conn, QStringLiteral("PRAGMA database_list"));
        if (!dbs)
            return fail(dbs.error());
        for (const catalog::Row &row : *dbs) {
            const QString name = text(row.at(1));
            if (name == u"temp") {
                auto used
                    = query(m_conn, QStringLiteral("SELECT count(*) FROM temp.sqlite_schema"));
                if (!used || used->isEmpty() || integer(used->first().at(0)) == 0)
                    continue;
            }
            ObjectNode n = node(kind::Database, name, true);
            n.badges.insert(QStringLiteral("file"), text(row.at(2)));
            out.append(n);
        }
        return out;
    }

    const ObjectRef &last = path.last();
    const QString db = path.find(kind::Database);
    if (path.depth() == 1 && last.kind == kind::Database) {
        return QList<ObjectNode>{groupNode(group::Tables), groupNode(group::Views),
                                 groupNode(group::Indexes), groupNode(group::Triggers)};
    }

    if (path.depth() == 2 && last.kind == kind::Group) {
        static const QHash<QString, std::pair<QString, QLatin1StringView>> types{
            {group::Tables, {QStringLiteral("table"), kind::Table}},
            {group::Views, {QStringLiteral("view"), kind::View}},
            {group::Indexes, {QStringLiteral("index"), kind::Index}},
            {group::Triggers, {QStringLiteral("trigger"), kind::Trigger}},
        };
        const auto type = types.value(last.name);
        if (type.first.isEmpty())
            return fail(badPath(path));
        auto rows = query(m_conn,
                          QStringLiteral("SELECT name, tbl_name FROM %1.sqlite_schema "
                                         "WHERE type = ? ORDER BY name")
                              .arg(ident(db)),
                          {Value::text(type.first)});
        if (!rows)
            return fail(rows.error());
        const bool expandable = type.second == kind::Table || type.second == kind::View;
        for (const catalog::Row &row : *rows) {
            ObjectNode n = node(type.second, text(row.at(0)), expandable);
            n.isSystem = n.ref.name.startsWith(u"sqlite_");
            if (!expandable)
                n.badges.insert(QStringLiteral("table"), text(row.at(1)));
            out.append(n);
        }
        return out;
    }

    const std::optional<Target> target = targetOf(path);
    if (!target)
        return fail(badPath(path));
    if (last.kind == kind::Table)
        return QList<ObjectNode>{groupNode(group::Columns), groupNode(group::Indexes),
                                 groupNode(group::Constraints), groupNode(group::Triggers)};
    if (last.kind == kind::View)
        return QList<ObjectNode>{groupNode(group::Columns)};
    if (last.kind != kind::Group)
        return out; // leaves

    Result<TableDef> def = table(path);
    if (!def)
        return fail(def.error());
    if (last.name == group::Columns) {
        for (const ColumnDef &c : def->columns) {
            const bool pk = def->primaryKey.contains(c.name);
            ObjectNode n = node(kind::Column, c.name, false, pk ? u"primary" : u"");
            n.badges.insert(QStringLiteral("type"), c.type);
            n.badges.insert(QStringLiteral("nullable"), c.nullable);
            out.append(n);
        }
    } else if (last.name == group::Indexes) {
        for (const IndexDef &i : def->indexes) {
            ObjectNode n = node(kind::Index, i.name, false);
            n.isSystem = i.name.startsWith(u"sqlite_autoindex_");
            n.badges.insert(QStringLiteral("columns"), i.columns.join(u", "));
            n.badges.insert(QStringLiteral("unique"), i.unique);
            out.append(n);
        }
    } else if (last.name == group::Constraints) {
        if (!def->primaryKey.isEmpty()) {
            ObjectNode n = node(kind::Constraint, QStringLiteral("PRIMARY KEY"), false, u"primary");
            n.displayName = QStringLiteral("PRIMARY KEY (%1)").arg(def->primaryKey.join(u", "));
            out.append(n);
        }
        for (const ConstraintDef &u : def->uniques) {
            ObjectNode n = node(kind::Constraint, u.name, false, u"unique");
            n.displayName = QStringLiteral("UNIQUE (%1)").arg(u.columns.join(u", "));
            out.append(n);
        }
        for (const ForeignKeyDef &fk : def->foreignKeys) {
            ObjectNode n = node(kind::Constraint, fk.name, false, u"foreign");
            n.displayName
                = QStringLiteral("(%1) → %2(%3)")
                      .arg(fk.columns.join(u", "), fk.refTable, fk.refColumns.join(u", "));
            out.append(n);
        }
    } else if (last.name == group::Triggers) {
        for (const TriggerDef &t : def->triggers) {
            ObjectNode n = node(kind::Trigger, t.name, false);
            n.badges.insert(QStringLiteral("timing"), t.timing);
            out.append(n);
        }
    }
    return out;
}

Result<TableDef> SqliteCatalog::table(const ObjectPath &path)
{
    const std::optional<Target> target = targetOf(path);
    if (!target)
        return fail(badPath(path));
    const QString db = ident(target->database);
    const QString name = literal(target->name);

    TableDef t;
    t.schema = target->database;
    t.name = target->name;
    t.kind = target->kind;

    auto info = query(m_conn, QStringLiteral("PRAGMA %1.table_list(%2)").arg(db, name));
    if (!info)
        return fail(info.error());
    if (info->isEmpty()) {
        DbError e;
        e.message = QStringLiteral("No such table or view: %1").arg(target->name);
        return fail(e);
    }
    // schema, name, type, ncol, wr, strict
    const catalog::Row &tl = info->first();
    t.kind = text(tl.at(2)) == u"view" ? QString(kind::View) : QString(kind::Table);
    if (boolean(tl.at(4)))
        t.options.insert(QStringLiteral("withoutRowid"), true);
    if (boolean(tl.at(5)))
        t.options.insert(QStringLiteral("strict"), true);

    // cid, name, type, notnull, dflt_value, pk, hidden
    auto cols = query(m_conn, QStringLiteral("PRAGMA %1.table_xinfo(%2)").arg(db, name));
    if (!cols)
        return fail(cols.error());
    QList<std::pair<qint64, QString>> pk;
    for (const catalog::Row &row : *cols) {
        ColumnDef c;
        c.name = text(row.at(1));
        c.type = text(row.at(2));
        c.kind = kindForDeclaredType(c.type);
        c.nullable = !boolean(row.at(3));
        c.defaultExpr = text(row.at(4));
        if (integer(row.at(5)) > 0)
            pk.append({integer(row.at(5)), c.name});
        const qint64 hidden = integer(row.at(6));
        if (hidden == 2 || hidden == 3) // generated VIRTUAL / STORED; the expression is in the DDL
            c.generatedExpr = hidden == 2 ? QStringLiteral("VIRTUAL") : QStringLiteral("STORED");
        t.columns.append(c);
    }
    std::ranges::sort(pk);
    for (const auto &[order, column] : pk)
        t.primaryKey.append(column);
    if (t.primaryKey.size() == 1 && !t.options.contains(QStringLiteral("withoutRowid"))) {
        // INTEGER PRIMARY KEY is the rowid: assigned automatically.
        for (ColumnDef &c : t.columns) {
            if (c.name == t.primaryKey.first()
                && c.type.compare(u"INTEGER", Qt::CaseInsensitive) == 0)
                c.autoIncrement = true;
        }
    }
    if (t.kind == kind::View)
        return t;

    // seq, name, unique, origin (c / u / pk), partial
    auto indexes = query(m_conn, QStringLiteral("PRAGMA %1.index_list(%2)").arg(db, name));
    if (!indexes)
        return fail(indexes.error());
    auto sqlOf = query(m_conn,
                       QStringLiteral("SELECT name, sql FROM %1.sqlite_schema WHERE "
                                      "tbl_name = ? AND type IN ('index', 'trigger')")
                           .arg(db),
                       {Value::text(target->name)});
    if (!sqlOf)
        return fail(sqlOf.error());
    QHash<QString, QString> definitions;
    for (const catalog::Row &row : *sqlOf)
        definitions.insert(text(row.at(0)), text(row.at(1)));
    for (const catalog::Row &row : *indexes) {
        IndexDef i;
        i.name = text(row.at(1));
        i.unique = boolean(row.at(2));
        const QString origin = text(row.at(3));
        i.primary = origin == u"pk";
        i.definition = definitions.value(i.name);
        if (boolean(row.at(4)))
            i.predicate = i.definition.section(QStringLiteral(" WHERE "), 1); // partial index
        // seqno, cid, name, desc, coll, key
        auto keys
            = query(m_conn, QStringLiteral("PRAGMA %1.index_xinfo(%2)").arg(db, literal(i.name)));
        if (!keys)
            return fail(keys.error());
        for (const catalog::Row &k : *keys) {
            if (!boolean(k.at(5)))
                continue;
            i.columns.append(integer(k.at(1)) == -2 ? QStringLiteral("<expression>")
                                                    : text(k.at(2)));
        }
        if (origin == u"u")
            t.uniques.append({.name = i.name, .columns = i.columns, .expression = {}});
        t.indexes.append(i);
    }
    std::ranges::reverse(t.indexes); // index_list lists the newest first

    // id, seq, table, from, to, on_update, on_delete, match
    auto fks = query(m_conn, QStringLiteral("PRAGMA %1.foreign_key_list(%2)").arg(db, name));
    if (!fks)
        return fail(fks.error());
    QMap<qint64, ForeignKeyDef> byId;
    for (const catalog::Row &row : *fks) {
        ForeignKeyDef &fk = byId[integer(row.at(0))];
        fk.name = QStringLiteral("fk_%1_%2").arg(target->name).arg(integer(row.at(0)));
        fk.refTable = text(row.at(2));
        fk.columns.append(text(row.at(3)));
        fk.refColumns.append(text(row.at(4))); // empty: the referenced table's primary key
        fk.onUpdate = text(row.at(5));
        fk.onDelete = text(row.at(6));
    }
    for (ForeignKeyDef &fk : byId)
        t.foreignKeys.append(fk);

    auto triggers = query(m_conn,
                          QStringLiteral("SELECT name, sql FROM %1.sqlite_schema WHERE "
                                         "type = 'trigger' AND tbl_name = ? ORDER BY name")
                              .arg(db),
                          {Value::text(target->name)});
    if (!triggers)
        return fail(triggers.error());
    for (const catalog::Row &row : *triggers) {
        TriggerDef tr;
        tr.name = text(row.at(0));
        tr.definition = text(row.at(1));
        parseTrigger(tr.definition, tr);
        t.triggers.append(tr);
    }
    return t;
}

Result<QString> SqliteCatalog::ddl(const ObjectPath &path)
{
    if (path.isRoot())
        return fail(badPath(path));
    const ObjectRef &last = path.last();
    const QString db = ident(path.find(kind::Database));
    static const QStringList kinds{kind::Table, kind::View, kind::Index, kind::Trigger};
    if (!kinds.contains(last.kind))
        return fail(badPath(path));
    auto rows
        = query(m_conn, QStringLiteral("SELECT sql FROM %1.sqlite_schema WHERE name = ?").arg(db),
                {Value::text(last.name)});
    if (!rows)
        return fail(rows.error());
    if (rows->isEmpty())
        return fail(badPath(path));
    QString out = text(rows->first().at(0)) + u';';
    if (last.kind == kind::Table) {
        auto related = query(m_conn,
                             QStringLiteral("SELECT sql FROM %1.sqlite_schema WHERE "
                                            "tbl_name = ? AND type IN ('index', 'trigger') "
                                            "AND sql IS NOT NULL ORDER BY type, name")
                                 .arg(db),
                             {Value::text(last.name)});
        if (!related)
            return fail(related.error());
        for (const catalog::Row &row : *related)
            out += QStringLiteral("\n\n") + text(row.at(0)) + u';';
    }
    return out + u'\n';
}

} // namespace owl::sqlite
