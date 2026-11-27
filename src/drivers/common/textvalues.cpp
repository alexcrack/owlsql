// SPDX-License-Identifier: GPL-3.0-or-later
#include "textvalues.h"

#include <limits>

namespace owl::drivers {

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

// Reads exactly `digits` decimal digits at `pos` (or at least that many when `atLeast`).
std::optional<qint64> readNumber(QByteArrayView s, qsizetype &pos, qsizetype digits, bool atLeast)
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

qsizetype utf8Cut(QByteArrayView s, qsizetype limit)
{
    if (s.size() <= limit)
        return s.size();
    qsizetype cut = limit;
    while (cut > 0 && (static_cast<unsigned char>(s.at(cut)) & 0xC0) == 0x80)
        --cut;
    return cut;
}

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

} // namespace owl::drivers
