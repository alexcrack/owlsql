// SPDX-License-Identifier: GPL-3.0-or-later
// Text-protocol helpers shared by the server drivers (PostgreSQL, MySQL/MariaDB): ISO date / clock
// parsing, UTF-8-safe preview truncation and ISO text for temporal parameters.
#pragma once

#include "owl/core/value.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QDate>

#include <optional>

namespace owl::drivers {

inline constexpr qint64 microsPerSecond = 1'000'000;
inline constexpr qint64 microsPerMinute = 60 * microsPerSecond;
inline constexpr qint64 microsPerHour = 60 * microsPerMinute;

bool isDigit(char c);
// Exactly `digits` decimal digits at `pos` (or at least that many when `atLeast`).
std::optional<qint64> readNumber(QByteArrayView s, qsizetype &pos, qsizetype digits,
                                 bool atLeast = false);
// ".ffffff" → microseconds (missing digits are zeros; more than 6 are ignored); 0 if absent.
qint64 readFraction(QByteArrayView s, qsizetype &pos);
// "H…H:MM:SS[.ffffff]" → microseconds (hours may exceed 24).
std::optional<qint64> readClock(QByteArrayView s, qsizetype &pos);
// "+HH[:MM[:SS]]" / "-HH…" → seconds east of UTC.
std::optional<int> readOffset(QByteArrayView s, qsizetype &pos);
// "YYYY-MM-DD" (years may have more than 4 digits); nullopt for invalid dates such as 0000-00-00.
std::optional<QDate> readDate(QByteArrayView s, qsizetype &pos);

// Cuts UTF-8 at `limit` bytes without splitting a multi-byte character.
qsizetype utf8Cut(QByteArrayView s, qsizetype limit);

// Text / Json / Unknown value from UTF-8, cut to the preview limit (specs/driver-api.md §6.3).
template<typename Make>
Value textValue(QByteArrayView s, qsizetype previewLimit, Make make)
{
    const qsizetype keep = utf8Cut(s, previewLimit);
    Value v = make(QString::fromUtf8(s.first(keep)));
    if (keep < s.size())
        v.setTruncated(s.size());
    return v;
}

QByteArray twoDigits(qint64 v);
// "HH:MM:SS[.ffffff]"
QByteArray clockText(qint64 micros);
// "+HH:MM[:SS]"
QByteArray offsetText(int offsetSeconds);
// ISO text of a temporal value: its original text when it has one, otherwise built from its parts
// ("2024-02-29 13:00:00.5+01:00", " BC" for negative years, "infinity").
QByteArray temporalText(ValueKind kind, const Temporal &t);

} // namespace owl::drivers
