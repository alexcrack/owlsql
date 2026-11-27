// SPDX-License-Identifier: GPL-3.0-or-later
#include "mysqlvalues.h"

#include "textvalues.h"

#include <QUuid>

namespace owl::mysql {

using namespace owl::drivers;

namespace {

constexpr qint64 microsPerDay = 24 * microsPerHour;

Value parseDate(QByteArrayView s)
{
    Temporal t;
    t.original = QString::fromLatin1(s);
    qsizetype pos = 0;
    if (auto date = readDate(s, pos); date && pos == s.size())
        t.date = *date;
    return Value::temporal(ValueKind::Date, std::move(t));
}

Value parseDateTime(QByteArrayView s)
{
    Temporal t;
    t.original = QString::fromLatin1(s);
    qsizetype pos = 0;
    auto date = readDate(s, pos);
    if (date && pos < s.size() && s.at(pos) == ' ') {
        ++pos;
        const auto clock = readClock(s, pos);
        if (clock && pos == s.size()) {
            t.date = *date;
            t.timeMicros = *clock;
        }
    }
    return Value::temporal(ValueKind::Timestamp, std::move(t));
}

Value parseTime(QByteArrayView s)
{
    QByteArrayView body = s;
    const bool negative = body.startsWith('-');
    if (negative)
        body = body.sliced(1);
    qsizetype pos = 0;
    const auto clock = readClock(body, pos);
    if (!clock || pos != body.size()) {
        Temporal t;
        t.original = QString::fromLatin1(s);
        return Value::temporal(ValueKind::Time, std::move(t));
    }
    if (!negative && *clock < microsPerDay)
        return Value::time(*clock, QString::fromLatin1(s));
    Interval iv;
    iv.micros = negative ? -*clock : *clock;
    iv.original = QString::fromLatin1(s);
    return Value::interval(std::move(iv));
}

} // namespace

Value parseValue(QByteArrayView s, ValueKind kind, qsizetype previewLimit)
{
    switch (kind) {
    case ValueKind::Bool:
        return Value::fromBool(s != QByteArrayView("0"));
    case ValueKind::Int: {
        bool ok = false;
        const qint64 v = s.toLongLong(&ok);
        return ok ? Value::fromInt(v) : Value::unknown(QString::fromLatin1(s));
    }
    case ValueKind::UInt: {
        bool ok = false;
        const quint64 v = s.toULongLong(&ok);
        return ok ? Value::fromUInt(v) : Value::unknown(QString::fromLatin1(s));
    }
    case ValueKind::Double: {
        bool ok = false;
        const double v = s.toDouble(&ok);
        return ok ? Value::fromDouble(v) : Value::unknown(QString::fromLatin1(s));
    }
    case ValueKind::Decimal:
        return Value::decimal(QString::fromLatin1(s));
    case ValueKind::Bytes: {
        const qsizetype keep = qMin(s.size(), previewLimit);
        Value v = Value::bytes(s.first(keep).toByteArray());
        if (keep < s.size())
            v.setTruncated(s.size());
        return v;
    }
    case ValueKind::Json:
        return textValue(s, previewLimit, [](QString t) { return Value::json(std::move(t)); });
    case ValueKind::Date:
        return parseDate(s);
    case ValueKind::Timestamp:
        return parseDateTime(s);
    case ValueKind::Time:
        return parseTime(s);
    case ValueKind::Unknown:
        return textValue(s, previewLimit, [](QString t) { return Value::unknown(std::move(t)); });
    default:
        return textValue(s, previewLimit, [](QString t) { return Value::text(std::move(t)); });
    }
}

std::optional<QByteArray> toParamText(const Value &value)
{
    switch (value.kind()) {
    case ValueKind::Null:
        return std::nullopt;
    case ValueKind::Bool:
        return value.toBool() ? QByteArray("1") : QByteArray("0");
    case ValueKind::Int:
        return QByteArray::number(value.toInt());
    case ValueKind::UInt:
        return QByteArray::number(value.toUInt());
    case ValueKind::Double:
        return QByteArray::number(value.toDouble(), 'g', 17);
    case ValueKind::Bytes:
        return value.toBytes();
    case ValueKind::Uuid:
        return value.toUuid().toByteArray(QUuid::WithoutBraces);
    case ValueKind::Date:
    case ValueKind::Time:
    case ValueKind::TimeTz:
    case ValueKind::Timestamp:
    case ValueKind::TimestampTz:
        return temporalText(value.kind(), value.toTemporal());
    case ValueKind::Interval: {
        const Interval iv = value.toInterval();
        if (!iv.original.isEmpty())
            return iv.original.toUtf8();
        const qint64 micros = iv.micros + qint64{iv.days} * microsPerDay;
        return (micros < 0 ? QByteArray("-") : QByteArray()) + clockText(qAbs(micros));
    }
    default:
        return value.toText().toUtf8();
    }
}

} // namespace owl::mysql
