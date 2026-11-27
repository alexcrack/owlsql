// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/catalogsupport.h"

namespace owl::catalog {

Result<QList<Row>> query(IConnection &connection, const QString &sql, QList<Value> params)
{
    Statement statement;
    statement.sql = sql;
    statement.params = std::move(params);
    statement.origin = StatementOrigin::Internal;
    ExecOptions options;
    options.previewLimitBytes = 16 * 1024 * 1024; // definitions are never cut short
    Result<std::unique_ptr<IResultCursor>> cursor = connection.execute(statement, options);
    if (!cursor)
        return fail(cursor.error());
    QList<Row> rows;
    const qsizetype columns = (*cursor)->columns().size();
    while (columns > 0) {
        Result<RowBatch> batch = (*cursor)->fetch(1024);
        if (!batch) {
            (*cursor)->close();
            return fail(batch.error());
        }
        if (batch->cells.isEmpty())
            break;
        for (qsizetype r = 0; r < batch->rowCount(); ++r)
            rows.append(batch->cells.mid(r * columns, columns));
    }
    (*cursor)->close();
    return rows;
}

QString text(const Value &v)
{
    switch (v.kind()) {
    case ValueKind::Null:
        return {};
    case ValueKind::Int:
        return QString::number(v.toInt());
    case ValueKind::UInt:
        return QString::number(v.toUInt());
    case ValueKind::Double:
        return QString::number(v.toDouble());
    case ValueKind::Bool:
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case ValueKind::Bytes:
        return QString::fromUtf8(v.toBytes()); // MySQL reports some names as binary strings
    default:
        return v.toText();
    }
}

qint64 integer(const Value &v)
{
    switch (v.kind()) {
    case ValueKind::Int:
        return v.toInt();
    case ValueKind::UInt:
        return static_cast<qint64>(v.toUInt());
    case ValueKind::Double:
        return static_cast<qint64>(v.toDouble());
    case ValueKind::Bool:
        return v.toBool() ? 1 : 0;
    default:
        return text(v).toLongLong();
    }
}

bool boolean(const Value &v)
{
    if (v.kind() == ValueKind::Bool)
        return v.toBool();
    if (v.kind() == ValueKind::Int || v.kind() == ValueKind::UInt)
        return integer(v) != 0;
    const QString t = text(v);
    return t.compare(u"t", Qt::CaseInsensitive) == 0 || t.compare(u"true", Qt::CaseInsensitive) == 0
        || t.compare(u"yes", Qt::CaseInsensitive) == 0 || t == u"1";
}

QStringList list(const Value &v, QChar separator)
{
    return text(v).split(separator, Qt::SkipEmptyParts);
}

} // namespace owl::catalog
