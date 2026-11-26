// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/value.h"

#include <QCborValue>
#include <QList>
#include <QString>

#include <optional>

namespace owl {

// Description of one result column (docs/specs/driver-api.md §2). The origin fields
// (schema / table / tableColumn / isPrimaryKey) are filled when the server reports them and make
// query results editable (specs/data-grid.md §5.1).
struct ColumnInfo
{
    QString name;
    ValueKind kind = ValueKind::Unknown;
    QString nativeType; // "jsonb", "varchar(255)", "tinyint(1) unsigned"
    quint32 nativeTypeId = 0; // PostgreSQL oid / MySQL field type; 0 if none
    int precision = -1;
    int scale = -1;
    int length = -1;
    bool nullable = true;
    QString schema;
    QString table;
    QString tableColumn;
    bool isPrimaryKey = false;

    bool operator==(const ColumnInfo &) const = default;

    QCborValue toCbor() const;
    static std::optional<ColumnInfo> fromCbor(const QCborValue &cbor);
};

// A chunk of result rows, row-major (docs/specs/driver-api.md §2).
struct RowBatch
{
    qsizetype firstRow = 0; // absolute index of the first row within the result
    qsizetype columnCount = 0;
    QList<Value> cells; // rowCount() * columnCount values

    qsizetype rowCount() const { return columnCount > 0 ? cells.size() / columnCount : 0; }
    const Value &at(qsizetype row, qsizetype column) const
    {
        return cells.at(row * columnCount + column);
    }

    bool operator==(const RowBatch &) const = default;

    QCborValue toCbor() const;
    static std::optional<RowBatch> fromCbor(const QCborValue &cbor);
};

} // namespace owl
