// SPDX-License-Identifier: GPL-3.0-or-later
#include "mysqlcatalog.h"

#include "mysqldialect.h"
#include "owl/core/catalogsupport.h"

#include <QRegularExpression>

namespace owl::mysql {

using catalog::integer;
using catalog::query;
using catalog::Row;
using catalog::text;

namespace {

const MySqlDialect &dialect()
{
    static const MySqlDialect d;
    return d;
}

QString ident(const QString &name)
{
    return dialect().quoteIdentifier(name);
}

QString qualified(const QString &db, const QString &name)
{
    return ident(db) + u'.' + ident(name);
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

bool isSystemDatabase(const QString &name)
{
    return name == u"information_schema" || name == u"mysql" || name == u"performance_schema"
        || name == u"sys";
}

ValueKind kindForDataType(const QString &dataType, const QString &columnType)
{
    static const QHash<QString, ValueKind> kinds{
        {QStringLiteral("tinyint"), ValueKind::Int},
        {QStringLiteral("smallint"), ValueKind::Int},
        {QStringLiteral("mediumint"), ValueKind::Int},
        {QStringLiteral("int"), ValueKind::Int},
        {QStringLiteral("integer"), ValueKind::Int},
        {QStringLiteral("bigint"), ValueKind::Int},
        {QStringLiteral("year"), ValueKind::Int},
        {QStringLiteral("decimal"), ValueKind::Decimal},
        {QStringLiteral("numeric"), ValueKind::Decimal},
        {QStringLiteral("float"), ValueKind::Double},
        {QStringLiteral("double"), ValueKind::Double},
        {QStringLiteral("date"), ValueKind::Date},
        {QStringLiteral("datetime"), ValueKind::Timestamp},
        {QStringLiteral("timestamp"), ValueKind::Timestamp},
        {QStringLiteral("time"), ValueKind::Time},
        {QStringLiteral("json"), ValueKind::Json},
        {QStringLiteral("binary"), ValueKind::Bytes},
        {QStringLiteral("varbinary"), ValueKind::Bytes},
        {QStringLiteral("tinyblob"), ValueKind::Bytes},
        {QStringLiteral("blob"), ValueKind::Bytes},
        {QStringLiteral("mediumblob"), ValueKind::Bytes},
        {QStringLiteral("longblob"), ValueKind::Bytes},
        {QStringLiteral("bit"), ValueKind::Bytes},
    };
    const QString type = dataType.toLower();
    if (type == u"bigint" && columnType.contains(u"unsigned"))
        return ValueKind::UInt;
    if (const auto it = kinds.constFind(type); it != kinds.cend())
        return *it;
    if (type.contains(u"geometry") || type.contains(u"point") || type.contains(u"polygon")
        || type.contains(u"linestring"))
        return ValueKind::Bytes;
    return ValueKind::Text; // char, varchar, text family, enum, set, uuid, inet6…
}

bool isNumericType(const QString &dataType)
{
    static const QStringList numeric{
        QStringLiteral("tinyint"), QStringLiteral("smallint"), QStringLiteral("mediumint"),
        QStringLiteral("int"),     QStringLiteral("integer"),  QStringLiteral("bigint"),
        QStringLiteral("decimal"), QStringLiteral("numeric"),  QStringLiteral("float"),
        QStringLiteral("double"),  QStringLiteral("year"),     QStringLiteral("bit")};
    return numeric.contains(dataType.toLower());
}

// The table or view a path points at (or lies below).
struct Target
{
    QString database;
    QString name;
    QString kind;
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

// SHOW CREATE text as a complete statement.
Result<QString> terminated(Result<QString> ddl)
{
    if (!ddl)
        return ddl;
    return *ddl + QStringLiteral(";\n");
}

} // namespace

bool MySqlCatalog::isMariaDb() const
{
    return m_conn.serverInfo().product == u"MariaDB";
}

Result<QList<ObjectNode>> MySqlCatalog::children(const ObjectPath &path)
{
    QList<ObjectNode> out;
    if (path.isRoot()) {
        auto rows = query(m_conn,
                          QStringLiteral("SELECT SCHEMA_NAME, DEFAULT_CHARACTER_SET_NAME, "
                                         "DEFAULT_COLLATION_NAME, SCHEMA_NAME = DATABASE() "
                                         "FROM information_schema.SCHEMATA ORDER BY SCHEMA_NAME"));
        if (!rows)
            return fail(rows.error());
        for (const Row &r : *rows) {
            ObjectNode n = node(kind::Database, text(r.at(0)), true);
            n.isSystem = isSystemDatabase(n.ref.name);
            n.badges.insert(QStringLiteral("charset"), text(r.at(1)));
            n.badges.insert(QStringLiteral("collation"), text(r.at(2)));
            if (integer(r.at(3)) == 1)
                n.badges.insert(QStringLiteral("current"), true);
            out.append(n);
        }
        return out;
    }

    const ObjectRef &last = path.last();
    const QString db = path.find(kind::Database);
    if (path.depth() == 1 && last.kind == kind::Database) {
        return QList<ObjectNode>{groupNode(group::Tables),    groupNode(group::Views),
                                 groupNode(group::Functions), groupNode(group::Procedures),
                                 groupNode(group::Triggers),  groupNode(group::Events)};
    }

    const QList<Value> dbParam{Value::text(db)};
    if (path.depth() == 2 && last.kind == kind::Group) {
        if (last.name == group::Tables || last.name == group::Views) {
            const bool tables = last.name == group::Tables;
            auto rows
                = query(m_conn,
                        QStringLiteral("SELECT TABLE_NAME, TABLE_ROWS, ENGINE, TABLE_COMMENT "
                                       "FROM information_schema.TABLES WHERE TABLE_SCHEMA = ? "
                                       "AND TABLE_TYPE %1 ORDER BY TABLE_NAME")
                            .arg(tables ? QStringLiteral("IN ('BASE TABLE', 'SYSTEM VERSIONED')")
                                        : QStringLiteral("= 'VIEW'")),
                        dbParam);
            if (!rows)
                return fail(rows.error());
            for (const Row &r : *rows) {
                ObjectNode n = node(tables ? kind::Table : kind::View, text(r.at(0)), true);
                n.isSystem = isSystemDatabase(db);
                if (tables) {
                    if (!r.at(1).isNull())
                        n.badges.insert(QStringLiteral("rows"), integer(r.at(1)));
                    n.badges.insert(QStringLiteral("engine"), text(r.at(2)));
                    addComment(n, r.at(3));
                }
                out.append(n);
            }
            return out;
        }
        if (last.name == group::Functions || last.name == group::Procedures) {
            const bool functions = last.name == group::Functions;
            auto rows
                = query(m_conn,
                        QStringLiteral("SELECT ROUTINE_NAME, ROUTINE_COMMENT "
                                       "FROM information_schema.ROUTINES WHERE ROUTINE_SCHEMA = ? "
                                       "AND ROUTINE_TYPE = ? ORDER BY ROUTINE_NAME"),
                        {Value::text(db),
                         Value::text(functions ? QStringLiteral("FUNCTION")
                                               : QStringLiteral("PROCEDURE"))});
            if (!rows)
                return fail(rows.error());
            for (const Row &r : *rows) {
                ObjectNode n
                    = node(functions ? kind::Function : kind::Procedure, text(r.at(0)), false);
                addComment(n, r.at(1));
                out.append(n);
            }
            return out;
        }
        if (last.name == group::Triggers) {
            auto rows
                = query(m_conn,
                        QStringLiteral("SELECT TRIGGER_NAME, EVENT_OBJECT_TABLE, ACTION_TIMING, "
                                       "EVENT_MANIPULATION FROM information_schema.TRIGGERS "
                                       "WHERE TRIGGER_SCHEMA = ? ORDER BY TRIGGER_NAME"),
                        dbParam);
            if (!rows)
                return fail(rows.error());
            for (const Row &r : *rows) {
                ObjectNode n = node(kind::Trigger, text(r.at(0)), false);
                n.badges.insert(QStringLiteral("table"), text(r.at(1)));
                n.badges.insert(QStringLiteral("timing"), text(r.at(2)));
                n.badges.insert(QStringLiteral("event"), text(r.at(3)));
                out.append(n);
            }
            return out;
        }
        if (last.name == group::Events) {
            auto rows
                = query(m_conn,
                        QStringLiteral("SELECT EVENT_NAME, STATUS, EVENT_COMMENT "
                                       "FROM information_schema.EVENTS WHERE EVENT_SCHEMA = ? "
                                       "ORDER BY EVENT_NAME"),
                        dbParam);
            if (!rows)
                return fail(rows.error());
            for (const Row &r : *rows) {
                ObjectNode n = node(kind::Event, text(r.at(0)), false);
                n.badges.insert(QStringLiteral("status"), text(r.at(1)));
                addComment(n, r.at(2));
                out.append(n);
            }
            return out;
        }
        return fail(badPath(path));
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
            if (!c.comment.isEmpty())
                n.badges.insert(QStringLiteral("comment"), c.comment);
            out.append(n);
        }
    } else if (last.name == group::Indexes) {
        for (const IndexDef &i : def->indexes) {
            ObjectNode n = node(kind::Index, i.name, false);
            n.badges.insert(QStringLiteral("columns"), i.columns.join(u", "));
            n.badges.insert(QStringLiteral("unique"), i.unique);
            n.badges.insert(QStringLiteral("primary"), i.primary);
            out.append(n);
        }
    } else if (last.name == group::Constraints) {
        if (!def->primaryKey.isEmpty()) {
            ObjectNode n = node(kind::Constraint, QStringLiteral("PRIMARY"), false, u"primary");
            n.displayName = QStringLiteral("PRIMARY KEY (%1)").arg(def->primaryKey.join(u", "));
            out.append(n);
        }
        for (const ConstraintDef &u : def->uniques)
            out.append(node(kind::Constraint, u.name, false, u"unique"));
        for (const ForeignKeyDef &fk : def->foreignKeys)
            out.append(node(kind::Constraint, fk.name, false, u"foreign"));
        for (const ConstraintDef &c : def->checks)
            out.append(node(kind::Constraint, c.name, false, u"check"));
    } else if (last.name == group::Triggers) {
        for (const TriggerDef &t : def->triggers) {
            ObjectNode n = node(kind::Trigger, t.name, false);
            n.badges.insert(QStringLiteral("timing"), t.timing);
            out.append(n);
        }
    }
    return out;
}

Result<TableDef> MySqlCatalog::table(const ObjectPath &path)
{
    const std::optional<Target> target = targetOf(path);
    if (!target)
        return fail(badPath(path));
    const QList<Value> params{Value::text(target->database), Value::text(target->name)};

    auto head = query(m_conn,
                      QStringLiteral("SELECT TABLE_TYPE, ENGINE, TABLE_ROWS, TABLE_COLLATION, "
                                     "TABLE_COMMENT, AUTO_INCREMENT FROM information_schema.TABLES "
                                     "WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ?"),
                      params);
    if (!head)
        return fail(head.error());
    if (head->isEmpty()) {
        DbError e;
        e.message = QStringLiteral("No such table or view: %1").arg(target->name);
        return fail(e);
    }
    const Row &h = head->first();
    TableDef t;
    t.schema = target->database;
    t.name = target->name;
    t.kind = text(h.at(0)) == u"VIEW" ? QString(kind::View) : QString(kind::Table);
    if (t.kind == kind::Table) {
        t.comment = text(h.at(4));
        t.options.insert(QStringLiteral("engine"), text(h.at(1)));
        t.options.insert(QStringLiteral("collation"), text(h.at(3)));
        if (!h.at(2).isNull())
            t.options.insert(QStringLiteral("rows"), integer(h.at(2)));
        if (!h.at(5).isNull())
            t.options.insert(QStringLiteral("autoIncrement"), integer(h.at(5)));
    }

    const bool mariadb = isMariaDb();
    auto cols = query(m_conn,
                      QStringLiteral("SELECT COLUMN_NAME, COLUMN_TYPE, DATA_TYPE, IS_NULLABLE, "
                                     "COLUMN_DEFAULT, EXTRA, COLLATION_NAME, COLUMN_COMMENT, "
                                     "GENERATION_EXPRESSION FROM information_schema.COLUMNS "
                                     "WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ? "
                                     "ORDER BY ORDINAL_POSITION"),
                      params);
    if (!cols)
        return fail(cols.error());
    for (const Row &r : *cols) {
        ColumnDef c;
        c.name = text(r.at(0));
        c.type = text(r.at(1));
        const QString dataType = text(r.at(2));
        c.kind = kindForDataType(dataType, c.type);
        c.nullable = text(r.at(3)) == u"YES";
        const QString extra = text(r.at(5));
        c.autoIncrement = extra.contains(u"auto_increment", Qt::CaseInsensitive);
        c.collation = text(r.at(6));
        c.comment = text(r.at(7));
        const bool generated = extra.contains(u"GENERATED", Qt::CaseInsensitive) // MySQL
            || extra.contains(u"VIRTUAL", Qt::CaseInsensitive) // MariaDB
            || extra.contains(u"PERSISTENT", Qt::CaseInsensitive) || extra.contains(u"STORED");
        if (generated && !extra.contains(u"DEFAULT_GENERATED")) {
            c.generatedExpr = text(r.at(8));
        } else if (!r.at(4).isNull()) {
            // MariaDB reports SQL expressions ('abc', NULL, current_timestamp()); MySQL reports
            // the plain value, or an expression when EXTRA says DEFAULT_GENERATED.
            const QString def = text(r.at(4));
            if (mariadb)
                c.defaultExpr = def == u"NULL" ? QString() : def;
            else if (extra.contains(u"DEFAULT_GENERATED") || isNumericType(dataType)
                     || def.compare(u"CURRENT_TIMESTAMP", Qt::CaseInsensitive) == 0)
                c.defaultExpr = def;
            else
                c.defaultExpr = dialect().quoteLiteral(Value::text(def));
        }
        t.columns.append(c);
    }
    if (t.kind == kind::View)
        return t;

    auto stats
        = query(m_conn,
                QStringLiteral("SELECT INDEX_NAME, NON_UNIQUE, COLUMN_NAME, INDEX_TYPE, SUB_PART "
                               "FROM information_schema.STATISTICS "
                               "WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ? "
                               "ORDER BY INDEX_NAME = 'PRIMARY' DESC, INDEX_NAME, SEQ_IN_INDEX"),
                params);
    if (!stats)
        return fail(stats.error());
    for (const Row &r : *stats) {
        const QString name = text(r.at(0));
        if (t.indexes.isEmpty() || t.indexes.last().name != name) {
            IndexDef i;
            i.name = name;
            i.unique = integer(r.at(1)) == 0;
            i.primary = name == u"PRIMARY";
            i.method = text(r.at(3));
            t.indexes.append(i);
        }
        QString column = r.at(2).isNull() ? QStringLiteral("<expression>") : text(r.at(2));
        if (!r.at(4).isNull())
            column += QStringLiteral("(%1)").arg(integer(r.at(4))); // prefix length
        t.indexes.last().columns.append(column);
    }
    for (const IndexDef &i : std::as_const(t.indexes)) {
        if (i.primary) {
            t.primaryKeyName = i.name;
            t.primaryKey = i.columns;
        } else if (i.unique) {
            t.uniques.append({.name = i.name, .columns = i.columns, .expression = {}});
        }
    }

    auto fks = query(
        m_conn,
        QStringLiteral("SELECT k.CONSTRAINT_NAME, k.COLUMN_NAME, k.REFERENCED_TABLE_SCHEMA, "
                       "k.REFERENCED_TABLE_NAME, k.REFERENCED_COLUMN_NAME, r.UPDATE_RULE, "
                       "r.DELETE_RULE FROM information_schema.KEY_COLUMN_USAGE k "
                       "JOIN information_schema.REFERENTIAL_CONSTRAINTS r "
                       "ON r.CONSTRAINT_SCHEMA = k.CONSTRAINT_SCHEMA "
                       "AND r.CONSTRAINT_NAME = k.CONSTRAINT_NAME "
                       "AND r.TABLE_NAME = k.TABLE_NAME "
                       "WHERE k.TABLE_SCHEMA = ? AND k.TABLE_NAME = ? "
                       "AND k.REFERENCED_TABLE_NAME IS NOT NULL "
                       "ORDER BY k.CONSTRAINT_NAME, k.ORDINAL_POSITION"),
        params);
    if (!fks)
        return fail(fks.error());
    for (const Row &r : *fks) {
        const QString name = text(r.at(0));
        if (t.foreignKeys.isEmpty() || t.foreignKeys.last().name != name) {
            t.foreignKeys.append({.name = name,
                                  .columns = {},
                                  .refSchema = text(r.at(2)),
                                  .refTable = text(r.at(3)),
                                  .refColumns = {},
                                  .onUpdate = text(r.at(5)),
                                  .onDelete = text(r.at(6))});
        }
        t.foreignKeys.last().columns.append(text(r.at(1)));
        t.foreignKeys.last().refColumns.append(text(r.at(4)));
    }

    // CHECK constraints: MySQL 8.0.16+, MariaDB (whose check names are unique per table only).
    const ServerInfo info = m_conn.serverInfo();
    if (mariadb || info.versionNumber >= 80016) {
        auto checks = query(
            m_conn,
            QStringLiteral("SELECT tc.CONSTRAINT_NAME, cc.CHECK_CLAUSE "
                           "FROM information_schema.TABLE_CONSTRAINTS tc "
                           "JOIN information_schema.CHECK_CONSTRAINTS cc "
                           "ON cc.CONSTRAINT_SCHEMA = tc.CONSTRAINT_SCHEMA "
                           "AND cc.CONSTRAINT_NAME = tc.CONSTRAINT_NAME %1"
                           "WHERE tc.TABLE_SCHEMA = ? AND tc.TABLE_NAME = ? "
                           "AND tc.CONSTRAINT_TYPE = 'CHECK' "
                           "ORDER BY tc.CONSTRAINT_NAME")
                .arg(mariadb ? QStringLiteral("AND cc.TABLE_NAME = tc.TABLE_NAME ") : QString()),
            params);
        if (!checks)
            return fail(checks.error());
        for (const Row &r : *checks)
            t.checks.append({.name = text(r.at(0)), .columns = {}, .expression = text(r.at(1))});
    }

    auto triggers = query(m_conn,
                          QStringLiteral("SELECT TRIGGER_NAME, ACTION_TIMING, EVENT_MANIPULATION, "
                                         "ACTION_STATEMENT FROM information_schema.TRIGGERS "
                                         "WHERE EVENT_OBJECT_SCHEMA = ? AND EVENT_OBJECT_TABLE = ? "
                                         "ORDER BY TRIGGER_NAME"),
                          params);
    if (!triggers)
        return fail(triggers.error());
    for (const Row &r : *triggers) {
        TriggerDef tr;
        tr.name = text(r.at(0));
        tr.timing = text(r.at(1));
        tr.events = {text(r.at(2))};
        tr.definition = QStringLiteral("CREATE TRIGGER %1 %2 %3 ON %4 FOR EACH ROW %5")
                            .arg(ident(tr.name), tr.timing, tr.events.first(),
                                 qualified(t.schema, t.name), text(r.at(3)));
        t.triggers.append(tr);
    }
    return t;
}

Result<QString> MySqlCatalog::showCreate(const QString &what, const QString &name, int column)
{
    auto rows = query(m_conn, QStringLiteral("SHOW CREATE %1 %2").arg(what, name));
    if (!rows)
        return fail(rows.error());
    if (rows->isEmpty() || rows->first().size() <= column)
        return fail(DbError::Category::Other, QStringLiteral("No definition for %1").arg(name));
    return text(rows->first().at(column));
}

Result<QString> MySqlCatalog::ddl(const ObjectPath &path)
{
    if (path.isRoot())
        return fail(badPath(path));
    const ObjectRef &last = path.last();
    const QString db = path.find(kind::Database);
    const QString name = qualified(db, last.name);

    if (last.kind == kind::Database)
        return terminated(showCreate(QStringLiteral("DATABASE"), ident(last.name), 1));
    if (last.kind == kind::View)
        return terminated(showCreate(QStringLiteral("VIEW"), name, 1));
    if (last.kind == kind::Function || last.kind == kind::Procedure) {
        const QString what = last.kind == kind::Function ? QStringLiteral("FUNCTION")
                                                         : QStringLiteral("PROCEDURE");
        return terminated(showCreate(what, name, 2));
    }
    if (last.kind == kind::Event)
        return terminated(showCreate(QStringLiteral("EVENT"), name, 3));
    if (last.kind == kind::Trigger && !targetOf(path))
        return terminated(showCreate(QStringLiteral("TRIGGER"), name, 2));

    if (last.kind == kind::Table) {
        Result<QString> create = showCreate(QStringLiteral("TABLE"), name, 1);
        if (!create)
            return create;
        QString out = *create + QStringLiteral(";\n");
        Result<TableDef> def = table(path);
        if (!def)
            return fail(def.error());
        for (const TriggerDef &tr : def->triggers) {
            Result<QString> trigger
                = showCreate(QStringLiteral("TRIGGER"), qualified(db, tr.name), 2);
            if (trigger)
                out += u'\n' + *trigger + QStringLiteral(";\n");
        }
        return out;
    }

    // Objects inside a table: built from its definition.
    const std::optional<Target> target = targetOf(path);
    if (!target)
        return fail(badPath(path));
    Result<TableDef> def = table(path);
    if (!def)
        return fail(def.error());
    const QString tableName = qualified(def->schema, def->name);
    // Index columns may carry a prefix length: "name(10)" → `name`(10).
    const auto columns = [](const QStringList &list) {
        static const QRegularExpression prefixed(QStringLiteral("^(.*)\\((\\d+)\\)$"));
        QStringList out;
        for (const QString &c : list) {
            if (c.startsWith(u'<')) {
                out << c;
            } else if (const QRegularExpressionMatch m = prefixed.match(c); m.hasMatch()) {
                out << ident(m.captured(1)) + u'(' + m.captured(2) + u')';
            } else {
                out << ident(c);
            }
        }
        return out.join(u", ");
    };
    if (last.kind == kind::Trigger) {
        return terminated(
            showCreate(QStringLiteral("TRIGGER"), qualified(def->schema, last.name), 2));
    }
    if (last.kind == kind::Index) {
        for (const IndexDef &i : def->indexes) {
            if (i.name != last.name)
                continue;
            if (i.primary)
                return QStringLiteral("ALTER TABLE %1 ADD PRIMARY KEY (%2);\n")
                    .arg(tableName, columns(i.columns));
            return QStringLiteral("CREATE %1INDEX %2 ON %3 (%4);\n")
                .arg(i.unique ? QStringLiteral("UNIQUE ") : QString(), ident(i.name), tableName,
                     columns(i.columns));
        }
    }
    if (last.kind == kind::Constraint) {
        if (last.name == u"PRIMARY")
            return QStringLiteral("ALTER TABLE %1 ADD PRIMARY KEY (%2);\n")
                .arg(tableName, columns(def->primaryKey));
        for (const ForeignKeyDef &fk : def->foreignKeys) {
            if (fk.name == last.name)
                return QStringLiteral("ALTER TABLE %1 ADD CONSTRAINT %2 FOREIGN KEY (%3) "
                                      "REFERENCES %4 (%5) ON UPDATE %6 ON DELETE %7;\n")
                    .arg(tableName, ident(fk.name), columns(fk.columns),
                         qualified(fk.refSchema, fk.refTable), columns(fk.refColumns), fk.onUpdate,
                         fk.onDelete);
        }
        for (const ConstraintDef &c : def->checks) {
            if (c.name == last.name)
                return QStringLiteral("ALTER TABLE %1 ADD CONSTRAINT %2 CHECK (%3);\n")
                    .arg(tableName, ident(c.name), c.expression);
        }
        for (const ConstraintDef &u : def->uniques) {
            if (u.name == last.name)
                return QStringLiteral("ALTER TABLE %1 ADD CONSTRAINT %2 UNIQUE (%3);\n")
                    .arg(tableName, ident(u.name), columns(u.columns));
        }
    }
    return fail(badPath(path));
}

} // namespace owl::mysql
