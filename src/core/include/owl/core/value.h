// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QCborValue>
#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QTime>
#include <QUuid>

#include <memory>
#include <optional>
#include <variant>

namespace owl {

// The kind of a database cell value (docs/specs/driver-api.md §2).
// The numbers are part of the CBOR wire format (Gateway, decision 009): never renumber, only
// append.
enum class ValueKind : quint8 {
    Null = 0,
    Bool = 1,
    Int = 2, // signed 64-bit
    UInt = 3, // unsigned 64-bit (MySQL BIGINT UNSIGNED)
    Double = 4,
    Decimal = 5, // exact textual form, never a double
    Text = 6,
    Bytes = 7,
    Date = 8,
    Time = 9,
    TimeTz = 10,
    Timestamp = 11, // without time zone
    TimestampTz = 12,
    Interval = 13,
    Uuid = 14,
    Json = 15, // raw JSON text
    Array = 16,
    Unknown = 17, // anything else, as the server's text (geometric, ranges, custom types...)
};

QString kindName(ValueKind kind);

// Temporal values keep the server's original text next to the parsed parts, so precision beyond
// what Qt types hold (microseconds), infinity, BC dates and MySQL's zero dates survive.
struct Temporal
{
    enum class Special : quint8 { None = 0, PlusInfinity = 1, MinusInfinity = 2 };

    QDate date; // Date, Timestamp*; invalid for "0000-00-00" and specials
    qint64 timeMicros = -1; // microseconds since midnight for Time*, Timestamp*; -1 = none
    std::optional<int> offsetSeconds; // TimeTz, TimestampTz: UTC offset of the value
    Special special = Special::None;
    QString original; // exactly as the server sent it (may be empty)

    bool operator==(const Temporal &) const = default;
};

// PostgreSQL-style interval; MySQL TIME used as a duration maps here too.
struct Interval
{
    qint32 months = 0;
    qint32 days = 0;
    qint64 micros = 0;
    QString original;

    bool operator==(const Interval &) const = default;
};

class Value
{
public:
    Value() = default; // Null

    static Value null() { return {}; }
    static Value fromBool(bool v);
    static Value fromInt(qint64 v);
    static Value fromUInt(quint64 v);
    static Value fromDouble(double v);
    static Value decimal(QString exactText);
    static Value text(QString v);
    static Value bytes(QByteArray v);
    static Value uuid(const QUuid &v);
    static Value json(QString rawJson);
    static Value unknown(QString serverText);
    static Value date(const QDate &date, QString original = {});
    static Value time(qint64 microsSinceMidnight, QString original = {});
    static Value timeTz(qint64 microsSinceMidnight, int offsetSeconds, QString original = {});
    static Value timestamp(const QDate &date, qint64 microsSinceMidnight, QString original = {});
    static Value timestampTz(const QDate &date, qint64 microsSinceMidnight, int offsetSeconds,
                             QString original = {});
    // Any temporal kind with explicit parts (specials, zero dates...).
    static Value temporal(ValueKind kind, Temporal parts);
    static Value interval(Interval parts);
    static Value array(QList<Value> elements);

    ValueKind kind() const;
    bool isNull() const { return kind() == ValueKind::Null; }
    bool isTemporal() const;

    // Accessors return a default value when the kind does not match.
    bool toBool() const;
    qint64 toInt() const;
    quint64 toUInt() const;
    double toDouble() const;
    // Text, Decimal, Json and Unknown: the string; temporal kinds and Interval: the original text.
    QString toText() const;
    QByteArray toBytes() const;
    QUuid toUuid() const;
    Temporal toTemporal() const;
    Interval toInterval() const;
    QList<Value> toArray() const;

    // Large values the driver cut short for the grid preview (docs/specs/driver-api.md §6.3).
    bool isTruncated() const { return m_fullSize >= 0; }
    qint64 fullSize() const { return m_fullSize; }
    void setTruncated(qint64 fullSize) { m_fullSize = fullSize; }

    // Deep equality; NaN equals NaN so that round trips compare equal.
    bool operator==(const Value &other) const;

    QCborValue toCbor() const;
    // std::nullopt when the CBOR does not describe a valid Value.
    static std::optional<Value> fromCbor(const QCborValue &cbor);

private:
    // One alternative per kind, so the variant index identifies the kind.
    struct DecimalText
    {
        QString text;
        bool operator==(const DecimalText &) const = default;
    };
    struct JsonText
    {
        QString text;
        bool operator==(const JsonText &) const = default;
    };
    struct UnknownText
    {
        QString text;
        bool operator==(const UnknownText &) const = default;
    };
    struct TemporalRef
    {
        ValueKind kind;
        std::shared_ptr<const Temporal> data;
        bool operator==(const TemporalRef &other) const
        {
            return kind == other.kind && *data == *other.data;
        }
    };
    using IntervalRef = std::shared_ptr<const Interval>;
    using ArrayRef = std::shared_ptr<const QList<Value>>;

    using Storage = std::variant<std::monostate, bool, qint64, quint64, double, DecimalText,
                                 QString, QByteArray, QUuid, JsonText, UnknownText, TemporalRef,
                                 IntervalRef, ArrayRef>;

    explicit Value(Storage storage)
        : m_storage(std::move(storage))
    { }

    Storage m_storage;
    qint64 m_fullSize = -1;
};

} // namespace owl
