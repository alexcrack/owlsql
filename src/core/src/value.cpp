// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/value.h"

#include <QCborArray>

#include <cmath>
#include <limits>

namespace owl {

namespace {

constexpr int lastKind = static_cast<int>(ValueKind::Unknown);

// Number of CBOR array elements for each kind, without the optional trailing fullSize.
int cborArity(ValueKind kind)
{
    switch (kind) {
    case ValueKind::Null:
        return 1;
    case ValueKind::Date:
    case ValueKind::Time:
    case ValueKind::TimeTz:
    case ValueKind::Timestamp:
    case ValueKind::TimestampTz:
        return 6;
    case ValueKind::Interval:
        return 5;
    default:
        return 2;
    }
}

bool isTemporalKind(ValueKind kind)
{
    return kind == ValueKind::Date || kind == ValueKind::Time || kind == ValueKind::TimeTz
        || kind == ValueKind::Timestamp || kind == ValueKind::TimestampTz;
}

} // namespace

QString kindName(ValueKind kind)
{
    switch (kind) {
    case ValueKind::Null:
        return QStringLiteral("null");
    case ValueKind::Bool:
        return QStringLiteral("bool");
    case ValueKind::Int:
        return QStringLiteral("int");
    case ValueKind::UInt:
        return QStringLiteral("uint");
    case ValueKind::Double:
        return QStringLiteral("double");
    case ValueKind::Decimal:
        return QStringLiteral("decimal");
    case ValueKind::Text:
        return QStringLiteral("text");
    case ValueKind::Bytes:
        return QStringLiteral("bytes");
    case ValueKind::Date:
        return QStringLiteral("date");
    case ValueKind::Time:
        return QStringLiteral("time");
    case ValueKind::TimeTz:
        return QStringLiteral("timetz");
    case ValueKind::Timestamp:
        return QStringLiteral("timestamp");
    case ValueKind::TimestampTz:
        return QStringLiteral("timestamptz");
    case ValueKind::Interval:
        return QStringLiteral("interval");
    case ValueKind::Uuid:
        return QStringLiteral("uuid");
    case ValueKind::Json:
        return QStringLiteral("json");
    case ValueKind::Array:
        return QStringLiteral("array");
    case ValueKind::Unknown:
        return QStringLiteral("unknown");
    }
    return {};
}

// --- construction ---------------------------------------------------------------------------

Value Value::fromBool(bool v)
{
    return Value(Storage(v));
}
Value Value::fromInt(qint64 v)
{
    return Value(Storage(v));
}
Value Value::fromUInt(quint64 v)
{
    return Value(Storage(v));
}
Value Value::fromDouble(double v)
{
    return Value(Storage(v));
}
Value Value::decimal(QString exactText)
{
    return Value(Storage(DecimalText{std::move(exactText)}));
}
Value Value::text(QString v)
{
    return Value(Storage(std::move(v)));
}
Value Value::bytes(QByteArray v)
{
    return Value(Storage(std::move(v)));
}
Value Value::uuid(const QUuid &v)
{
    return Value(Storage(v));
}
Value Value::json(QString rawJson)
{
    return Value(Storage(JsonText{std::move(rawJson)}));
}
Value Value::unknown(QString serverText)
{
    return Value(Storage(UnknownText{std::move(serverText)}));
}

Value Value::temporal(ValueKind kind, Temporal parts)
{
    Q_ASSERT(isTemporalKind(kind));
    return Value(Storage(
        TemporalRef{.kind = kind, .data = std::make_shared<const Temporal>(std::move(parts))}));
}

Value Value::date(const QDate &date, QString original)
{
    Temporal t;
    t.date = date;
    t.original = std::move(original);
    return temporal(ValueKind::Date, std::move(t));
}

Value Value::time(qint64 microsSinceMidnight, QString original)
{
    Temporal t;
    t.timeMicros = microsSinceMidnight;
    t.original = std::move(original);
    return temporal(ValueKind::Time, std::move(t));
}

Value Value::timeTz(qint64 microsSinceMidnight, int offsetSeconds, QString original)
{
    Temporal t;
    t.timeMicros = microsSinceMidnight;
    t.offsetSeconds = offsetSeconds;
    t.original = std::move(original);
    return temporal(ValueKind::TimeTz, std::move(t));
}

Value Value::timestamp(const QDate &date, qint64 microsSinceMidnight, QString original)
{
    Temporal t;
    t.date = date;
    t.timeMicros = microsSinceMidnight;
    t.original = std::move(original);
    return temporal(ValueKind::Timestamp, std::move(t));
}

Value Value::timestampTz(const QDate &date, qint64 microsSinceMidnight, int offsetSeconds,
                         QString original)
{
    Temporal t;
    t.date = date;
    t.timeMicros = microsSinceMidnight;
    t.offsetSeconds = offsetSeconds;
    t.original = std::move(original);
    return temporal(ValueKind::TimestampTz, std::move(t));
}

Value Value::interval(Interval parts)
{
    return Value(Storage(std::make_shared<const Interval>(std::move(parts))));
}

Value Value::array(QList<Value> elements)
{
    return Value(Storage(std::make_shared<const QList<Value>>(std::move(elements))));
}

// --- inspection -----------------------------------------------------------------------------

ValueKind Value::kind() const
{
    struct Visitor
    {
        ValueKind operator()(std::monostate) const { return ValueKind::Null; }
        ValueKind operator()(bool) const { return ValueKind::Bool; }
        ValueKind operator()(qint64) const { return ValueKind::Int; }
        ValueKind operator()(quint64) const { return ValueKind::UInt; }
        ValueKind operator()(double) const { return ValueKind::Double; }
        ValueKind operator()(const DecimalText &) const { return ValueKind::Decimal; }
        ValueKind operator()(const QString &) const { return ValueKind::Text; }
        ValueKind operator()(const QByteArray &) const { return ValueKind::Bytes; }
        ValueKind operator()(const QUuid &) const { return ValueKind::Uuid; }
        ValueKind operator()(const JsonText &) const { return ValueKind::Json; }
        ValueKind operator()(const UnknownText &) const { return ValueKind::Unknown; }
        ValueKind operator()(const TemporalRef &t) const { return t.kind; }
        ValueKind operator()(const IntervalRef &) const { return ValueKind::Interval; }
        ValueKind operator()(const ArrayRef &) const { return ValueKind::Array; }
    };
    return std::visit(Visitor{}, m_storage);
}

bool Value::isTemporal() const
{
    return std::holds_alternative<TemporalRef>(m_storage);
}

bool Value::toBool() const
{
    const auto *v = std::get_if<bool>(&m_storage);
    return v ? *v : false;
}

qint64 Value::toInt() const
{
    const auto *v = std::get_if<qint64>(&m_storage);
    return v ? *v : 0;
}

quint64 Value::toUInt() const
{
    const auto *v = std::get_if<quint64>(&m_storage);
    return v ? *v : 0;
}

double Value::toDouble() const
{
    const auto *v = std::get_if<double>(&m_storage);
    return v ? *v : 0.0;
}

QString Value::toText() const
{
    if (const auto *v = std::get_if<QString>(&m_storage))
        return *v;
    if (const auto *v = std::get_if<DecimalText>(&m_storage))
        return v->text;
    if (const auto *v = std::get_if<JsonText>(&m_storage))
        return v->text;
    if (const auto *v = std::get_if<UnknownText>(&m_storage))
        return v->text;
    if (const auto *v = std::get_if<TemporalRef>(&m_storage))
        return v->data->original;
    if (const auto *v = std::get_if<IntervalRef>(&m_storage))
        return (*v)->original;
    return {};
}

QByteArray Value::toBytes() const
{
    const auto *v = std::get_if<QByteArray>(&m_storage);
    return v ? *v : QByteArray();
}

QUuid Value::toUuid() const
{
    const auto *v = std::get_if<QUuid>(&m_storage);
    return v ? *v : QUuid();
}

Temporal Value::toTemporal() const
{
    const auto *v = std::get_if<TemporalRef>(&m_storage);
    return v ? *v->data : Temporal();
}

Interval Value::toInterval() const
{
    const auto *v = std::get_if<IntervalRef>(&m_storage);
    return v ? **v : Interval();
}

QList<Value> Value::toArray() const
{
    const auto *v = std::get_if<ArrayRef>(&m_storage);
    return v ? **v : QList<Value>();
}

bool Value::operator==(const Value &other) const
{
    if (m_fullSize != other.m_fullSize || m_storage.index() != other.m_storage.index())
        return false;
    if (const auto *a = std::get_if<double>(&m_storage)) {
        const double b = std::get<double>(other.m_storage);
        return (std::isnan(*a) && std::isnan(b)) || *a == b;
    }
    if (const auto *a = std::get_if<TemporalRef>(&m_storage)) {
        const auto &b = std::get<TemporalRef>(other.m_storage);
        return a->kind == b.kind && *a->data == *b.data;
    }
    if (const auto *a = std::get_if<IntervalRef>(&m_storage))
        return **a == *std::get<IntervalRef>(other.m_storage);
    if (const auto *a = std::get_if<ArrayRef>(&m_storage))
        return **a == *std::get<ArrayRef>(other.m_storage);
    return m_storage == other.m_storage;
}

// --- CBOR -----------------------------------------------------------------------------------
// Layout: [kindCode, payload..., fullSize?] — see docs/specs/driver-api.md §2.
//   scalars/text/bytes/uuid: [k, value]   uint above INT64_MAX: decimal string
//   temporal: [k, original, julianDay|null, timeMicros|null, offsetSeconds|null, special]
//   interval: [k, original, months, days, micros]      array: [k, [elements...]]

QCborValue Value::toCbor() const
{
    const ValueKind k = kind();
    QCborArray out{static_cast<int>(k)};
    switch (k) {
    case ValueKind::Null:
        break;
    case ValueKind::Bool:
        out.append(toBool());
        break;
    case ValueKind::Int:
        out.append(toInt());
        break;
    case ValueKind::UInt: {
        const quint64 v = toUInt();
        if (v <= static_cast<quint64>(std::numeric_limits<qint64>::max()))
            out.append(static_cast<qint64>(v));
        else
            out.append(QString::number(v));
        break;
    }
    case ValueKind::Double:
        out.append(toDouble());
        break;
    case ValueKind::Decimal:
    case ValueKind::Text:
    case ValueKind::Json:
    case ValueKind::Unknown:
        out.append(toText());
        break;
    case ValueKind::Bytes:
        out.append(toBytes());
        break;
    case ValueKind::Uuid:
        out.append(toUuid().toRfc4122());
        break;
    case ValueKind::Date:
    case ValueKind::Time:
    case ValueKind::TimeTz:
    case ValueKind::Timestamp:
    case ValueKind::TimestampTz: {
        const Temporal t = toTemporal();
        out.append(t.original);
        out.append(t.date.isValid() ? QCborValue(t.date.toJulianDay()) : QCborValue(nullptr));
        out.append(t.timeMicros >= 0 ? QCborValue(t.timeMicros) : QCborValue(nullptr));
        out.append(t.offsetSeconds ? QCborValue(*t.offsetSeconds) : QCborValue(nullptr));
        out.append(static_cast<int>(t.special));
        break;
    }
    case ValueKind::Interval: {
        const Interval i = toInterval();
        out.append(i.original);
        out.append(i.months);
        out.append(i.days);
        out.append(i.micros);
        break;
    }
    case ValueKind::Array: {
        QCborArray elements;
        for (const Value &element : **std::get_if<ArrayRef>(&m_storage))
            elements.append(element.toCbor());
        out.append(elements);
        break;
    }
    }
    if (isTruncated())
        out.append(m_fullSize);
    return out;
}

std::optional<Value> Value::fromCbor(const QCborValue &cbor)
{
    if (!cbor.isArray())
        return std::nullopt;
    const QCborArray a = cbor.toArray();
    if (a.isEmpty() || !a.at(0).isInteger())
        return std::nullopt;
    const qint64 code = a.at(0).toInteger();
    if (code < 0 || code > lastKind)
        return std::nullopt;
    const auto k = static_cast<ValueKind>(code);
    const qsizetype arity = cborArity(k);
    if (a.size() != arity && a.size() != arity + 1)
        return std::nullopt;
    const QCborValue p = a.size() > 1 ? a.at(1) : QCborValue();

    std::optional<Value> v;
    switch (k) {
    case ValueKind::Null:
        v = Value();
        break;
    case ValueKind::Bool:
        if (p.isBool())
            v = fromBool(p.toBool());
        break;
    case ValueKind::Int:
        if (p.isInteger())
            v = fromInt(p.toInteger());
        break;
    case ValueKind::UInt:
        if (p.isInteger() && p.toInteger() >= 0) {
            v = fromUInt(static_cast<quint64>(p.toInteger()));
        } else if (p.isString()) {
            bool ok = false;
            const quint64 n = p.toString().toULongLong(&ok);
            if (ok)
                v = fromUInt(n);
        }
        break;
    case ValueKind::Double:
        if (p.isDouble() || p.isInteger())
            v = fromDouble(p.toDouble());
        break;
    case ValueKind::Decimal:
        if (p.isString())
            v = decimal(p.toString());
        break;
    case ValueKind::Text:
        if (p.isString())
            v = text(p.toString());
        break;
    case ValueKind::Json:
        if (p.isString())
            v = json(p.toString());
        break;
    case ValueKind::Unknown:
        if (p.isString())
            v = unknown(p.toString());
        break;
    case ValueKind::Bytes:
        if (p.isByteArray())
            v = bytes(p.toByteArray());
        break;
    case ValueKind::Uuid:
        if (p.isByteArray() && p.toByteArray().size() == 16)
            v = uuid(QUuid::fromRfc4122(p.toByteArray()));
        break;
    case ValueKind::Date:
    case ValueKind::Time:
    case ValueKind::TimeTz:
    case ValueKind::Timestamp:
    case ValueKind::TimestampTz: {
        const QCborValue day = a.at(2), micros = a.at(3), offset = a.at(4), special = a.at(5);
        const auto intOrNull = [](const QCborValue &x) {
            return x.isInteger() || x.isNull();
        };
        if (!p.isString() || !intOrNull(day) || !intOrNull(micros) || !intOrNull(offset)
            || !special.isInteger() || special.toInteger() < 0 || special.toInteger() > 2)
            break;
        Temporal t;
        t.original = p.toString();
        if (day.isInteger())
            t.date = QDate::fromJulianDay(day.toInteger());
        if (micros.isInteger())
            t.timeMicros = micros.toInteger();
        if (offset.isInteger())
            t.offsetSeconds = static_cast<int>(offset.toInteger());
        t.special = static_cast<Temporal::Special>(special.toInteger());
        v = temporal(k, std::move(t));
        break;
    }
    case ValueKind::Interval:
        if (p.isString() && a.at(2).isInteger() && a.at(3).isInteger() && a.at(4).isInteger())
            v = interval({.months = static_cast<qint32>(a.at(2).toInteger()),
                          .days = static_cast<qint32>(a.at(3).toInteger()),
                          .micros = a.at(4).toInteger(),
                          .original = p.toString()});
        break;
    case ValueKind::Array: {
        if (!p.isArray())
            break;
        QList<Value> elements;
        const QCborArray items = p.toArray();
        elements.reserve(items.size());
        for (const auto &item : items) {
            std::optional<Value> element = fromCbor(item);
            if (!element)
                return std::nullopt;
            elements.append(std::move(*element));
        }
        v = array(std::move(elements));
        break;
    }
    }

    if (v && a.size() == arity + 1) {
        if (!a.at(arity).isInteger() || a.at(arity).toInteger() < 0)
            return std::nullopt;
        v->setTruncated(a.at(arity).toInteger());
    }
    return v;
}

} // namespace owl
