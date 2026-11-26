// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/resultset.h"

#include "cborutil.h"

#include <QCborArray>
#include <QCborMap>

using namespace Qt::StringLiterals;

namespace owl {

// ColumnInfo: map with short keys; absent keys take their defaults (forward compatible).
QCborValue ColumnInfo::toCbor() const
{
    QCborMap m;
    m.insert(u"n"_s, name);
    m.insert(u"k"_s, static_cast<int>(kind));
    m.insert(u"t"_s, nativeType);
    m.insert(u"ti"_s, static_cast<qint64>(nativeTypeId));
    m.insert(u"p"_s, precision);
    m.insert(u"s"_s, scale);
    m.insert(u"l"_s, length);
    m.insert(u"nl"_s, nullable);
    m.insert(u"sc"_s, schema);
    m.insert(u"tb"_s, table);
    m.insert(u"tc"_s, tableColumn);
    m.insert(u"pk"_s, isPrimaryKey);
    return m;
}

std::optional<ColumnInfo> ColumnInfo::fromCbor(const QCborValue &cbor)
{
    if (!cbor.isMap())
        return std::nullopt;
    const QCborMap m = cbor.toMap();
    ColumnInfo c;
    qint64 kind = static_cast<int>(c.kind);
    qint64 typeId = 0;
    qint64 precision = c.precision, scale = c.scale, length = c.length;
    const bool ok = cbor::readString(m, u"n"_s, c.name) && cbor::readInt(m, u"k"_s, kind)
        && kind >= 0 && kind <= static_cast<int>(ValueKind::Unknown)
        && cbor::readString(m, u"t"_s, c.nativeType) && cbor::readInt(m, u"ti"_s, typeId)
        && typeId >= 0 && typeId <= 0xFFFFFFFFLL && cbor::readInt(m, u"p"_s, precision)
        && cbor::readInt(m, u"s"_s, scale) && cbor::readInt(m, u"l"_s, length)
        && cbor::readBool(m, u"nl"_s, c.nullable) && cbor::readString(m, u"sc"_s, c.schema)
        && cbor::readString(m, u"tb"_s, c.table) && cbor::readString(m, u"tc"_s, c.tableColumn)
        && cbor::readBool(m, u"pk"_s, c.isPrimaryKey);
    if (!ok)
        return std::nullopt;
    c.kind = static_cast<ValueKind>(kind);
    c.nativeTypeId = static_cast<quint32>(typeId);
    c.precision = static_cast<int>(precision);
    c.scale = static_cast<int>(scale);
    c.length = static_cast<int>(length);
    return c;
}

// RowBatch: [firstRow, columnCount, [cells...]]
QCborValue RowBatch::toCbor() const
{
    QCborArray values;
    for (const Value &cell : cells)
        values.append(cell.toCbor());
    return QCborArray{static_cast<qint64>(firstRow), static_cast<qint64>(columnCount), values};
}

std::optional<RowBatch> RowBatch::fromCbor(const QCborValue &cbor)
{
    if (!cbor.isArray())
        return std::nullopt;
    const QCborArray a = cbor.toArray();
    if (a.size() != 3 || !a.at(0).isInteger() || !a.at(1).isInteger() || !a.at(2).isArray())
        return std::nullopt;
    RowBatch batch;
    batch.firstRow = a.at(0).toInteger();
    batch.columnCount = a.at(1).toInteger();
    const QCborArray values = a.at(2).toArray();
    if (batch.firstRow < 0 || batch.columnCount < 0
        || (batch.columnCount == 0 ? !values.isEmpty() : values.size() % batch.columnCount != 0))
        return std::nullopt;
    batch.cells.reserve(values.size());
    for (const auto &item : values) {
        std::optional<Value> v = Value::fromCbor(item);
        if (!v)
            return std::nullopt;
        batch.cells.append(std::move(*v));
    }
    return batch;
}

} // namespace owl
