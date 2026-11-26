// SPDX-License-Identifier: GPL-3.0-or-later
#include "pgvalues.h"

#include <QUuid>

#include <array>
#include <cmath>
#include <limits>
#include <optional>

namespace owl::postgres {

namespace {

constexpr qint64 microsPerSecond = 1'000'000;
constexpr qint64 microsPerMinute = 60 * microsPerSecond;
constexpr qint64 microsPerHour = 60 * microsPerMinute;

struct Builtin
{
    constexpr Builtin(quint32 typeOid, const char *typeName, ValueKind valueKind,
                      quint32 element = 0)
        : oid(typeOid)
        , name(typeName)
        , kind(valueKind)
        , elementOid(element)
    { }
    quint32 oid;
    const char *name;
    ValueKind kind;
    quint32 elementOid;
};

constexpr auto builtins = std::to_array<Builtin>({
    {oid::Bool, "bool", ValueKind::Bool, 0},
    {oid::Bytea, "bytea", ValueKind::Bytes, 0},
    {oid::Char, "char", ValueKind::Text, 0},
    {oid::Name, "name", ValueKind::Text, 0},
    {oid::Int8, "int8", ValueKind::Int, 0},
    {oid::Int2, "int2", ValueKind::Int, 0},
    {oid::Int4, "int4", ValueKind::Int, 0},
    {oid::Text, "text", ValueKind::Text, 0},
    {oid::Oid, "oid", ValueKind::Int, 0},
    {oid::Xid, "xid", ValueKind::Int, 0},
    {oid::Json, "json", ValueKind::Json, 0},
    {oid::Float4, "float4", ValueKind::Double, 0},
    {oid::Float8, "float8", ValueKind::Double, 0},
    {oid::Unknown, "unknown", ValueKind::Text, 0},
    {oid::Bpchar, "bpchar", ValueKind::Text, 0},
    {oid::Varchar, "varchar", ValueKind::Text, 0},
    {oid::Date, "date", ValueKind::Date, 0},
    {oid::Time, "time", ValueKind::Time, 0},
    {oid::Timestamp, "timestamp", ValueKind::Timestamp, 0},
    {oid::TimestampTz, "timestamptz", ValueKind::TimestampTz, 0},
    {oid::Interval, "interval", ValueKind::Interval, 0},
    {oid::TimeTz, "timetz", ValueKind::TimeTz, 0},
    {oid::Numeric, "numeric", ValueKind::Decimal, 0},
    {oid::Uuid, "uuid", ValueKind::Uuid, 0},
    {oid::Jsonb, "jsonb", ValueKind::Json, 0},
    {oid::Xid8, "xid8", ValueKind::UInt, 0},
    // Arrays
    {1000, "_bool", ValueKind::Array, oid::Bool},
    {1001, "_bytea", ValueKind::Array, oid::Bytea},
    {1002, "_char", ValueKind::Array, oid::Char},
    {1003, "_name", ValueKind::Array, oid::Name},
    {1016, "_int8", ValueKind::Array, oid::Int8},
    {1005, "_int2", ValueKind::Array, oid::Int2},
    {1007, "_int4", ValueKind::Array, oid::Int4},
    {1009, "_text", ValueKind::Array, oid::Text},
    {1028, "_oid", ValueKind::Array, oid::Oid},
    {1011, "_xid", ValueKind::Array, oid::Xid},
    {199, "_json", ValueKind::Array, oid::Json},
    {1021, "_float4", ValueKind::Array, oid::Float4},
    {1022, "_float8", ValueKind::Array, oid::Float8},
    {1014, "_bpchar", ValueKind::Array, oid::Bpchar},
    {1015, "_varchar", ValueKind::Array, oid::Varchar},
    {1182, "_date", ValueKind::Array, oid::Date},
    {1183, "_time", ValueKind::Array, oid::Time},
    {1115, "_timestamp", ValueKind::Array, oid::Timestamp},
    {1185, "_timestamptz", ValueKind::Array, oid::TimestampTz},
    {1187, "_interval", ValueKind::Array, oid::Interval},
    {1270, "_timetz", ValueKind::Array, oid::TimeTz},
    {1231, "_numeric", ValueKind::Array, oid::Numeric},
    {2951, "_uuid", ValueKind::Array, oid::Uuid},
    {3807, "_jsonb", ValueKind::Array, oid::Jsonb},
    {271, "_xid8", ValueKind::Array, oid::Xid8},
    // No dedicated kind (shown as server text)
    {790, "money", ValueKind::Unknown, 0},
    {869, "inet", ValueKind::Unknown, 0},
    {650, "cidr", ValueKind::Unknown, 0},
    {829, "macaddr", ValueKind::Unknown, 0},
    {1560, "bit", ValueKind::Unknown, 0},
    {1562, "varbit", ValueKind::Unknown, 0},
    {142, "xml", ValueKind::Unknown, 0},
    {600, "point", ValueKind::Unknown, 0},
    {603, "box", ValueKind::Unknown, 0},
    {3614, "tsvector", ValueKind::Unknown, 0},
    {3615, "tsquery", ValueKind::Unknown, 0},
    {4072, "jsonpath", ValueKind::Unknown, 0},
    {2249, "record", ValueKind::Unknown, 0},
    {2278, "void", ValueKind::Unknown, 0},
    {2205, "regclass", ValueKind::Unknown, 0},
    {2206, "regtype", ValueKind::Unknown, 0},
    {24, "regproc", ValueKind::Unknown, 0},
    {3904, "int4range", ValueKind::Unknown, 0},
});

constexpr quint32 boxOid = 603; // the only built-in type whose array delimiter is ';'

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

// Reads exactly `digits` decimal digits at `pos` (or at least that many when `atLeast`).
std::optional<qint64> readNumber(QByteArrayView s, qsizetype &pos, qsizetype digits,
                                 bool atLeast = false)
{
    const qsizetype start = pos;
    while (pos < s.size() && isDigit(s.at(pos)) && (atLeast || pos - start < digits))
        ++pos;
    if (pos - start < digits || pos == start)
        return std::nullopt;
    qint64 v = 0;
    for (qsizetype i = start; i < pos; ++i) {
        if (v > (std::numeric_limits<qint64>::max() - 9) / 10)
            return std::nullopt;
        v = v * 10 + (s.at(i) - '0');
    }
    return v;
}

// ".ffffff" → microseconds (missing digits are zeros; more than 6 are ignored).
qint64 readFraction(QByteArrayView s, qsizetype &pos)
{
    if (pos >= s.size() || s.at(pos) != '.')
        return 0;
    ++pos;
    qint64 micros = 0;
    int digits = 0;
    while (pos < s.size() && isDigit(s.at(pos))) {
        if (digits < 6) {
            micros = micros * 10 + (s.at(pos) - '0');
            ++digits;
        }
        ++pos;
    }
    while (digits++ < 6)
        micros *= 10;
    return micros;
}

// "HH:MM:SS[.ffffff]" → microseconds since midnight (24:00:00 is valid in PostgreSQL).
std::optional<qint64> readClock(QByteArrayView s, qsizetype &pos)
{
    const auto h = readNumber(s, pos, 2, true);
    if (!h || pos >= s.size() || s.at(pos) != ':')
        return std::nullopt;
    ++pos;
    const auto m = readNumber(s, pos, 2);
    if (!m || pos >= s.size() || s.at(pos) != ':')
        return std::nullopt;
    ++pos;
    const auto sec = readNumber(s, pos, 2);
    if (!sec)
        return std::nullopt;
    return *h * microsPerHour + *m * microsPerMinute + *sec * microsPerSecond
        + readFraction(s, pos);
}

// "+HH[:MM[:SS]]" / "-HH…" → seconds east of UTC.
std::optional<int> readOffset(QByteArrayView s, qsizetype &pos)
{
    if (pos >= s.size() || (s.at(pos) != '+' && s.at(pos) != '-'))
        return std::nullopt;
    const int sign = s.at(pos) == '-' ? -1 : 1;
    ++pos;
    const auto h = readNumber(s, pos, 2);
    if (!h)
        return std::nullopt;
    qint64 seconds = *h * 3600;
    for (const int unit : {60, 1}) {
        if (pos < s.size() && s.at(pos) == ':') {
            ++pos;
            const auto part = readNumber(s, pos, 2);
            if (!part)
                return std::nullopt;
            seconds += *part * unit;
        }
    }
    return sign * static_cast<int>(seconds);
}

// "YYYY-MM-DD" (years may have more than 4 digits).
std::optional<QDate> readDate(QByteArrayView s, qsizetype &pos)
{
    const auto y = readNumber(s, pos, 4, true);
    if (!y || pos >= s.size() || s.at(pos) != '-')
        return std::nullopt;
    ++pos;
    const auto m = readNumber(s, pos, 2);
    if (!m || pos >= s.size() || s.at(pos) != '-')
        return std::nullopt;
    ++pos;
    const auto d = readNumber(s, pos, 2);
    if (!d || *y > std::numeric_limits<int>::max())
        return std::nullopt;
    QDate date(static_cast<int>(*y), static_cast<int>(*m), static_cast<int>(*d));
    if (!date.isValid())
        return std::nullopt;
    return date;
}

// " BC" suffix: year N BC is proleptic year -N in QDate (which has no year 0).
bool readEra(QByteArrayView s, qsizetype &pos, QDate &date)
{
    if (s.sliced(pos) == QByteArrayView(" BC")) {
        date = QDate(-date.year(), date.month(), date.day());
        pos = s.size();
        return date.isValid();
    }
    return true;
}

std::optional<Temporal::Special> special(QByteArrayView s)
{
    if (s == QByteArrayView("infinity"))
        return Temporal::Special::PlusInfinity;
    if (s == QByteArrayView("-infinity"))
        return Temporal::Special::MinusInfinity;
    return std::nullopt;
}

// Cuts UTF-8 at `limit` bytes without splitting a multi-byte character.
qsizetype utf8Cut(QByteArrayView s, qsizetype limit)
{
    if (s.size() <= limit)
        return s.size();
    qsizetype cut = limit;
    while (cut > 0 && (static_cast<unsigned char>(s.at(cut)) & 0xC0) == 0x80)
        --cut;
    return cut;
}

template<typename Make>
Value textValue(QByteArrayView s, qsizetype previewLimit, Make make)
{
    const qsizetype keep = utf8Cut(s, previewLimit);
    Value v = make(QString::fromUtf8(s.first(keep)));
    if (keep < s.size())
        v.setTruncated(s.size());
    return v;
}

int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// bytea output: hex ("\x00ff", the default) or escape format ("abc\000\\").
Value parseBytea(QByteArrayView s, qsizetype previewLimit)
{
    QByteArray out;
    qint64 fullSize = 0;
    if (s.startsWith("\\x")) {
        fullSize = (s.size() - 2) / 2;
        const qsizetype keep = qMin<qsizetype>(fullSize, previewLimit);
        out.resize(keep);
        for (qsizetype i = 0; i < keep; ++i) {
            const int hi = hexValue(s.at(2 + 2 * i));
            const int lo = hexValue(s.at(3 + 2 * i));
            if (hi < 0 || lo < 0)
                return Value::unknown(QString::fromLatin1(s));
            out[i] = static_cast<char>((hi << 4) | lo);
        }
    } else {
        for (qsizetype i = 0; i < s.size(); ++i) {
            char c = s.at(i);
            if (c == '\\') {
                if (i + 1 < s.size() && s.at(i + 1) == '\\') {
                    ++i;
                } else if (i + 3 < s.size()) {
                    c = static_cast<char>(((s.at(i + 1) - '0') << 6) | ((s.at(i + 2) - '0') << 3)
                                          | (s.at(i + 3) - '0'));
                    i += 3;
                }
            }
            out.append(c);
        }
        fullSize = out.size();
        out.truncate(previewLimit);
    }
    Value v = Value::bytes(out);
    if (out.size() < fullSize)
        v.setTruncated(fullSize);
    return v;
}

Value parseDouble(QByteArrayView s)
{
    if (s == QByteArrayView("Infinity"))
        return Value::fromDouble(std::numeric_limits<double>::infinity());
    if (s == QByteArrayView("-Infinity"))
        return Value::fromDouble(-std::numeric_limits<double>::infinity());
    if (s == QByteArrayView("NaN"))
        return Value::fromDouble(std::numeric_limits<double>::quiet_NaN());
    bool ok = false;
    const double d = s.toDouble(&ok);
    return ok ? Value::fromDouble(d) : Value::unknown(QString::fromLatin1(s));
}

// Array literal parser: {a,"b c",NULL,{nested}} with an optional "[1:2]=" bounds prefix.
class ArrayParser
{
public:
    ArrayParser(QByteArrayView s, quint32 elementOid, const TypeRegistry &types,
                qsizetype previewLimit)
        : m_s(s)
        , m_elementOid(elementOid)
        , m_types(types)
        , m_limit(previewLimit)
    { }

    std::optional<QList<Value>> run()
    {
        if (m_s.startsWith('[')) {
            const qsizetype eq = m_s.indexOf('=');
            if (eq < 0)
                return std::nullopt;
            m_pos = eq + 1;
        }
        auto result = list();
        if (!result || m_pos != m_s.size())
            return std::nullopt;
        return result;
    }

private:
    std::optional<QList<Value>> list()
    {
        if (m_pos >= m_s.size() || m_s.at(m_pos) != '{')
            return std::nullopt;
        ++m_pos;
        QList<Value> out;
        if (m_pos < m_s.size() && m_s.at(m_pos) == '}') {
            ++m_pos;
            return out;
        }
        for (;;) {
            if (m_pos >= m_s.size())
                return std::nullopt;
            const char c = m_s.at(m_pos);
            if (c == '{') {
                auto inner = list();
                if (!inner)
                    return std::nullopt;
                out.append(Value::array(std::move(*inner)));
            } else if (c == '"') {
                auto element = quoted();
                if (!element)
                    return std::nullopt;
                out.append(parseValue(*element, m_elementOid, m_types, m_limit));
            } else {
                const qsizetype start = m_pos;
                while (m_pos < m_s.size() && m_s.at(m_pos) != ',' && m_s.at(m_pos) != '}')
                    ++m_pos;
                const QByteArrayView raw = m_s.sliced(start, m_pos - start);
                if (raw.compare("NULL", Qt::CaseInsensitive) == 0)
                    out.append(Value());
                else
                    out.append(parseValue(raw, m_elementOid, m_types, m_limit));
            }
            if (m_pos >= m_s.size())
                return std::nullopt;
            if (m_s.at(m_pos) == '}') {
                ++m_pos;
                return out;
            }
            if (m_s.at(m_pos) != ',')
                return std::nullopt;
            ++m_pos;
        }
    }

    std::optional<QByteArray> quoted()
    {
        ++m_pos; // opening quote
        QByteArray out;
        while (m_pos < m_s.size()) {
            const char c = m_s.at(m_pos++);
            if (c == '\\' && m_pos < m_s.size()) {
                out.append(m_s.at(m_pos++));
            } else if (c == '"') {
                return out;
            } else {
                out.append(c);
            }
        }
        return std::nullopt;
    }

    QByteArrayView m_s;
    quint32 m_elementOid;
    const TypeRegistry &m_types;
    qsizetype m_limit;
    qsizetype m_pos = 0;
};

QByteArray twoDigits(qint64 v)
{
    return QByteArray::number(v).rightJustified(2, '0');
}

QByteArray clockText(qint64 micros)
{
    const qint64 seconds = micros / microsPerSecond;
    QByteArray out = twoDigits(seconds / 3600) + ':' + twoDigits((seconds / 60) % 60) + ':'
        + twoDigits(seconds % 60);
    if (const qint64 fraction = micros % microsPerSecond; fraction != 0)
        out += '.' + QByteArray::number(fraction).rightJustified(6, '0');
    return out;
}

QByteArray offsetText(int offsetSeconds)
{
    const int a = qAbs(offsetSeconds);
    QByteArray out
        = (offsetSeconds < 0 ? "-" : "+") + twoDigits(a / 3600) + ':' + twoDigits((a / 60) % 60);
    if (a % 60 != 0)
        out += ':' + twoDigits(a % 60);
    return out;
}

QByteArray temporalText(ValueKind kind, const Temporal &t)
{
    if (!t.original.isEmpty())
        return t.original.toUtf8();
    if (t.special == Temporal::Special::PlusInfinity)
        return "infinity";
    if (t.special == Temporal::Special::MinusInfinity)
        return "-infinity";
    QByteArray out;
    const bool hasDate
        = kind == ValueKind::Date || kind == ValueKind::Timestamp || kind == ValueKind::TimestampTz;
    if (hasDate && t.date.isValid()) {
        out += QByteArray::number(qAbs(t.date.year())).rightJustified(4, '0') + '-'
            + twoDigits(t.date.month()) + '-' + twoDigits(t.date.day());
    }
    if (kind != ValueKind::Date && t.timeMicros >= 0) {
        if (!out.isEmpty())
            out += ' ';
        out += clockText(t.timeMicros);
        if (t.offsetSeconds)
            out += offsetText(*t.offsetSeconds);
    }
    if (hasDate && t.date.isValid() && t.date.year() < 0)
        out += " BC";
    return out;
}

QByteArray intervalText(const Interval &iv)
{
    if (!iv.original.isEmpty())
        return iv.original.toUtf8();
    QByteArray out
        = QByteArray::number(iv.months) + " mons " + QByteArray::number(iv.days) + " days ";
    if (iv.micros < 0)
        out += '-';
    return out + clockText(qAbs(iv.micros));
}

QByteArray arrayElementText(const Value &v)
{
    if (v.isNull())
        return "NULL";
    QByteArray text = toParamText(v).value_or(QByteArray()); // not NULL here
    if (v.kind() == ValueKind::Array)
        return text;
    QByteArray quoted = "\"";
    for (const char c : text) {
        if (c == '"' || c == '\\')
            quoted += '\\';
        quoted += c;
    }
    return quoted + '"';
}

} // namespace

std::optional<QByteArray> toParamText(const Value &value)
{
    switch (value.kind()) {
    case ValueKind::Null:
        return std::nullopt;
    case ValueKind::Bool:
        return value.toBool() ? QByteArray("t") : QByteArray("f");
    case ValueKind::Int:
        return QByteArray::number(value.toInt());
    case ValueKind::UInt:
        return QByteArray::number(value.toUInt());
    case ValueKind::Double: {
        const double d = value.toDouble();
        if (std::isnan(d))
            return QByteArray("NaN");
        if (std::isinf(d))
            return d > 0 ? QByteArray("Infinity") : QByteArray("-Infinity");
        return QByteArray::number(d, 'g', 17);
    }
    case ValueKind::Bytes:
        return "\\x" + value.toBytes().toHex();
    case ValueKind::Uuid:
        return value.toUuid().toByteArray(QUuid::WithoutBraces);
    case ValueKind::Date:
    case ValueKind::Time:
    case ValueKind::TimeTz:
    case ValueKind::Timestamp:
    case ValueKind::TimestampTz:
        return temporalText(value.kind(), value.toTemporal());
    case ValueKind::Interval:
        return intervalText(value.toInterval());
    case ValueKind::Array: {
        QByteArray out = "{";
        const QList<Value> elements = value.toArray();
        for (qsizetype i = 0; i < elements.size(); ++i) {
            if (i > 0)
                out += ',';
            out += arrayElementText(elements.at(i));
        }
        return out + '}';
    }
    default:
        return value.toText().toUtf8();
    }
}

TypeRegistry::TypeRegistry()
{
    for (const Builtin &b : builtins)
        m_types.insert(b.oid,
                       TypeInfo{.name = QString::fromLatin1(b.name),
                                .kind = b.kind,
                                .elementOid = b.elementOid});
}

const TypeInfo *TypeRegistry::find(quint32 typeOid) const
{
    const auto it = m_types.constFind(typeOid);
    return it == m_types.cend() ? nullptr : &*it;
}

ValueKind TypeRegistry::kind(quint32 typeOid) const
{
    const TypeInfo *info = find(typeOid);
    return info ? info->kind : ValueKind::Unknown;
}

QString TypeRegistry::displayName(quint32 typeOid, int typmod) const
{
    const TypeInfo *info = find(typeOid);
    if (!info)
        return {};
    if (info->kind == ValueKind::Array && info->elementOid != 0) {
        const QString element = displayName(info->elementOid, typmod);
        if (!element.isEmpty())
            return element + QStringLiteral("[]");
    }
    const TypeModifiers mods = decodeTypmod(typeOid, typmod);
    if (mods.length >= 0)
        return QStringLiteral("%1(%2)").arg(info->name).arg(mods.length);
    if (mods.precision >= 0 && mods.scale >= 0)
        return QStringLiteral("%1(%2,%3)").arg(info->name).arg(mods.precision).arg(mods.scale);
    if (mods.precision >= 0)
        return QStringLiteral("%1(%2)").arg(info->name).arg(mods.precision);
    return info->name;
}

TypeModifiers decodeTypmod(quint32 typeOid, int typmod)
{
    TypeModifiers m;
    if (typmod < 0)
        return m;
    switch (typeOid) {
    case oid::Numeric:
        if (typmod >= 4) {
            const int t = typmod - 4;
            m.precision = (t >> 16) & 0xffff;
            m.scale = ((t & 0x7ff) ^ 1024) - 1024; // PostgreSQL 15+ allows negative scale
        }
        break;
    case oid::Varchar:
    case oid::Bpchar:
        if (typmod >= 4)
            m.length = typmod - 4;
        break;
    case 1560: // bit
    case 1562: // varbit
        m.length = typmod;
        break;
    case oid::Time:
    case oid::TimeTz:
    case oid::Timestamp:
    case oid::TimestampTz:
        m.precision = typmod;
        break;
    case oid::Interval:
        if ((typmod & 0xffff) != 0xffff)
            m.precision = typmod & 0xffff;
        break;
    default:
        break;
    }
    return m;
}

Value parseDate(QByteArrayView s)
{
    Temporal t;
    t.original = QString::fromLatin1(s);
    if (const auto sp = special(s)) {
        t.special = *sp;
    } else {
        qsizetype pos = 0;
        auto date = readDate(s, pos);
        if (date && readEra(s, pos, *date) && pos == s.size())
            t.date = *date;
    }
    return Value::temporal(ValueKind::Date, std::move(t));
}

Value parseTime(QByteArrayView s, bool withZone)
{
    Temporal t;
    t.original = QString::fromLatin1(s);
    qsizetype pos = 0;
    const auto clock = readClock(s, pos);
    std::optional<int> offset;
    if (withZone)
        offset = readOffset(s, pos);
    if (clock && pos == s.size() && (!withZone || offset)) {
        t.timeMicros = *clock;
        t.offsetSeconds = offset;
    }
    return Value::temporal(withZone ? ValueKind::TimeTz : ValueKind::Time, std::move(t));
}

Value parseTimestamp(QByteArrayView s, bool withZone)
{
    Temporal t;
    t.original = QString::fromLatin1(s);
    const ValueKind kind = withZone ? ValueKind::TimestampTz : ValueKind::Timestamp;
    if (const auto sp = special(s)) {
        t.special = *sp;
        return Value::temporal(kind, std::move(t));
    }
    qsizetype pos = 0;
    auto date = readDate(s, pos);
    if (!date || pos >= s.size() || s.at(pos) != ' ')
        return Value::temporal(kind, std::move(t));
    ++pos;
    const auto clock = readClock(s, pos);
    std::optional<int> offset;
    if (withZone)
        offset = readOffset(s, pos);
    if (clock && (!withZone || offset) && readEra(s, pos, *date) && pos == s.size()) {
        t.date = *date;
        t.timeMicros = *clock;
        t.offsetSeconds = offset;
    }
    return Value::temporal(kind, std::move(t));
}

// IntervalStyle "postgres" (set by the driver): "1 year 2 mons -3 days +04:05:06.5".
// Other styles keep only the original text.
Value parseInterval(QByteArrayView s)
{
    Interval iv;
    iv.original = QString::fromLatin1(s);
    Interval parsed;
    const QList<QByteArrayView> words = [&] {
        QList<QByteArrayView> out;
        for (qsizetype start = 0; start < s.size();) {
            qsizetype end = s.indexOf(' ', start);
            if (end < 0)
                end = s.size();
            if (end > start)
                out.append(s.sliced(start, end - start));
            start = end + 1;
        }
        return out;
    }();
    bool ok = !words.isEmpty();
    for (qsizetype i = 0; ok && i < words.size(); ++i) {
        QByteArrayView w = words.at(i);
        if (w.contains(':')) {
            qint64 sign = 1;
            if (w.startsWith('-') || w.startsWith('+')) {
                sign = w.startsWith('-') ? -1 : 1;
                w = w.sliced(1);
            }
            qsizetype pos = 0;
            const auto clock = readClock(w, pos);
            ok = clock && pos == w.size();
            if (ok)
                parsed.micros += sign * *clock;
            continue;
        }
        if (i + 1 >= words.size()) {
            ok = false;
            break;
        }
        bool numberOk = false;
        const qint64 n = w.toLongLong(&numberOk);
        const QByteArrayView unit = words.at(++i);
        if (numberOk && (unit == QByteArrayView("year") || unit == QByteArrayView("years"))) {
            parsed.months += static_cast<qint32>(n * 12);
        } else if (numberOk && (unit == QByteArrayView("mon") || unit == QByteArrayView("mons"))) {
            parsed.months += static_cast<qint32>(n);
        } else if (numberOk && (unit == QByteArrayView("day") || unit == QByteArrayView("days"))) {
            parsed.days += static_cast<qint32>(n);
        } else {
            ok = false;
        }
    }
    if (ok) {
        parsed.original = iv.original;
        return Value::interval(std::move(parsed));
    }
    return Value::interval(std::move(iv));
}

Value parseArray(QByteArrayView s, quint32 elementOid, const TypeRegistry &types,
                 qsizetype previewLimit)
{
    if (elementOid == boxOid) // ';'-delimited; keep as text
        return Value::unknown(QString::fromUtf8(s));
    auto elements = ArrayParser(s, elementOid, types, previewLimit).run();
    if (!elements)
        return Value::unknown(QString::fromUtf8(s));
    return Value::array(std::move(*elements));
}

Value parseValue(QByteArrayView s, quint32 typeOid, const TypeRegistry &types,
                 qsizetype previewLimit)
{
    const TypeInfo *info = types.find(typeOid);
    const ValueKind kind = info ? info->kind : ValueKind::Unknown;
    switch (kind) {
    case ValueKind::Bool:
        return Value::fromBool(s == QByteArrayView("t"));
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
    case ValueKind::Double:
        return parseDouble(s);
    case ValueKind::Decimal:
        return Value::decimal(QString::fromLatin1(s));
    case ValueKind::Text:
        return textValue(s, previewLimit, [](QString t) { return Value::text(std::move(t)); });
    case ValueKind::Json:
        return textValue(s, previewLimit, [](QString t) { return Value::json(std::move(t)); });
    case ValueKind::Bytes:
        return parseBytea(s, previewLimit);
    case ValueKind::Uuid: {
        const QUuid uuid = QUuid::fromString(QLatin1StringView(s));
        return uuid.isNull() && s != QByteArrayView("00000000-0000-0000-0000-000000000000")
            ? Value::unknown(QString::fromLatin1(s))
            : Value::uuid(uuid);
    }
    case ValueKind::Date:
        return parseDate(s);
    case ValueKind::Time:
        return parseTime(s, false);
    case ValueKind::TimeTz:
        return parseTime(s, true);
    case ValueKind::Timestamp:
        return parseTimestamp(s, false);
    case ValueKind::TimestampTz:
        return parseTimestamp(s, true);
    case ValueKind::Interval:
        return parseInterval(s);
    case ValueKind::Array:
        return parseArray(s, info->elementOid, types, previewLimit);
    default:
        return textValue(s, previewLimit, [](QString t) { return Value::unknown(std::move(t)); });
    }
}

} // namespace owl::postgres
