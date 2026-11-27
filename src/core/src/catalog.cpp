// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/catalog.h"

#include <QHashFunctions>

namespace owl {

ObjectPath ObjectPath::child(ObjectRef ref) const
{
    ObjectPath p = *this;
    p.parts.append(std::move(ref));
    return p;
}

bool ObjectPath::startsWith(const ObjectPath &other) const
{
    if (other.parts.size() > parts.size())
        return false;
    for (qsizetype i = 0; i < other.parts.size(); ++i) {
        if (!(parts.at(i) == other.parts.at(i)))
            return false;
    }
    return true;
}

QString ObjectPath::find(QLatin1StringView kind) const
{
    for (const ObjectRef &r : parts) {
        if (r.kind == kind)
            return r.name;
    }
    return {};
}

QString ObjectPath::toString() const
{
    QStringList out;
    for (const ObjectRef &r : parts)
        out << r.kind + u':' + r.name + (r.oid.isEmpty() ? QString() : u'#' + r.oid);
    return out.join(u'/');
}

size_t qHash(const ObjectRef &ref, size_t seed)
{
    return qHashMulti(seed, ref.kind, ref.name, ref.oid);
}

size_t qHash(const ObjectPath &path, size_t seed)
{
    return qHashRange(path.parts.cbegin(), path.parts.cend(), seed);
}

const ColumnDef *TableDef::column(QStringView columnName) const
{
    for (const ColumnDef &c : columns) {
        if (c.name == columnName)
            return &c;
    }
    return nullptr;
}

QString iconFor(QAnyStringView kind, QAnyStringView detail)
{
    if (kind == kind::Database)
        return QStringLiteral("database");
    if (kind == kind::Schema)
        return QStringLiteral("schema");
    if (kind == kind::Group)
        return QStringLiteral("folder");
    if (kind == kind::Table)
        return QStringLiteral("table");
    if (kind == kind::View)
        return QStringLiteral("eye");
    if (kind == kind::MaterializedView)
        return QStringLiteral("table-shortcut");
    if (kind == kind::Sequence)
        return QStringLiteral("list-numbers");
    if (kind == kind::Function)
        return QStringLiteral("math-function");
    if (kind == kind::Procedure)
        return QStringLiteral("terminal-2");
    if (kind == kind::Type)
        return QStringLiteral("box");
    if (kind == kind::Extension)
        return QStringLiteral("puzzle");
    if (kind == kind::Event)
        return QStringLiteral("calendar-event");
    if (kind == kind::Column)
        return detail == u"primary" ? QStringLiteral("key") : QStringLiteral("columns");
    if (kind == kind::Index)
        return QStringLiteral("list-details");
    if (kind == kind::Trigger)
        return QStringLiteral("bolt");
    if (kind == kind::Constraint) {
        if (detail == u"primary")
            return QStringLiteral("key");
        if (detail == u"foreign")
            return QStringLiteral("link");
        if (detail == u"unique")
            return QStringLiteral("fingerprint");
        if (detail == u"check")
            return QStringLiteral("circle-check");
        return QStringLiteral("lock");
    }
    return QStringLiteral("file-text");
}

ObjectNode groupNode(QLatin1StringView group, bool hasChildren)
{
    ObjectNode n;
    n.ref = {.kind = kind::Group, .name = group, .oid = {}};
    // Display names are translated in the UI layer; this is the fallback.
    QString title = group;
    if (group == group::MaterializedViews)
        title = QStringLiteral("materialized views");
    title[0] = title.at(0).toUpper();
    n.displayName = title;
    n.icon = iconFor(kind::Group);
    n.hasChildren = hasChildren;
    return n;
}

} // namespace owl
