// SPDX-License-Identifier: GPL-3.0-or-later
// MySQL / MariaDB values: text-protocol cells → owl::Value, owl::Value → parameter text
// (specs/driver-api.md §11). No connector calls, so unit-tested without a server.
#pragma once

#include "owl/core/value.h"

#include <QByteArrayView>

#include <optional>

namespace owl::mysql {

// Parses one non-NULL cell. `kind` is the column's kind (from the field type, flags and
// character set); `previewLimit` bytes cap text and binary values (§6.3).
//  - Date / Timestamp: "YYYY-MM-DD[ HH:MM:SS[.ffffff]]"; zero dates keep only their text.
//  - Time: MySQL TIME is also a duration (-838:59:59 … 838:59:59): values outside a day become
//    an Interval (decision 158).
Value parseValue(QByteArrayView text, ValueKind kind, qsizetype previewLimit);

// A bound parameter as text, for kinds without a native MYSQL_BIND type (temporals, decimals,
// text, json, uuid…). std::nullopt for NULL.
std::optional<QByteArray> toParamText(const Value &value);

} // namespace owl::mysql
