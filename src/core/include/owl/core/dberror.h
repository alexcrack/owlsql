// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCborValue>
#include <QString>

#include <optional>

namespace owl {

// A failure reported by a driver or the server (docs/specs/driver-api.md §3.3).
struct DbError
{
    // Numbers are part of the CBOR wire format: never renumber, only append.
    enum class Category : quint8 {
        Other = 0,
        Connection = 1, // cannot connect / network
        Auth = 2, // authentication or authorization at login
        Syntax = 3, // statement could not be parsed
        Constraint = 4, // unique / foreign key / check / not-null violation
        Permission = 5, // missing privilege for an operation
        Timeout = 6,
        Cancelled = 7, // cancelled by the user
        Lost = 8, // connection dropped mid-way: triggers the reconnect policy (§7)
        Unsupported = 9, // the driver/engine cannot do this
    };

    Category category = Category::Other;
    QString sqlState; // "42P01", "HY000"
    int nativeCode = 0; // engine-specific error number (MySQL errno...)
    QString message;
    QString detail;
    QString hint;
    int position = -1; // character offset in the statement (editor squiggle), -1 if none

    bool operator==(const DbError &) const = default;

    // "Syntax: relation \"users\" does not exist (42P01)"
    QString toString() const;
    static QString categoryName(Category category);

    QCborValue toCbor() const;
    static std::optional<DbError> fromCbor(const QCborValue &cbor);
};

// Server-side notices and warnings produced while a statement runs (§3.3).
struct ServerMessage
{
    enum class Severity : quint8 { Debug = 0, Info = 1, Notice = 2, Warning = 3 };

    Severity severity = Severity::Notice;
    QString text;
    QString sqlState;

    bool operator==(const ServerMessage &) const = default;

    QCborValue toCbor() const;
    static std::optional<ServerMessage> fromCbor(const QCborValue &cbor);
};

} // namespace owl
