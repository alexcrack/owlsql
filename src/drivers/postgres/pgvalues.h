// SPDX-License-Identifier: GPL-3.0-or-later
// PostgreSQL text-format values → owl::Value (specs/driver-api.md §11). Pure functions, no libpq
// connection needed, so they are unit-tested without a server.
#pragma once

#include "owl/core/value.h"

#include <QByteArrayView>
#include <QHash>
#include <QString>

#include <optional>

namespace owl::postgres {

// Built-in type OIDs (src/include/catalog/pg_type.dat). Stable across server versions.
namespace oid {
inline constexpr quint32 Bool = 16;
inline constexpr quint32 Bytea = 17;
inline constexpr quint32 Char = 18;
inline constexpr quint32 Name = 19;
inline constexpr quint32 Int8 = 20;
inline constexpr quint32 Int2 = 21;
inline constexpr quint32 Int4 = 23;
inline constexpr quint32 Text = 25;
inline constexpr quint32 Oid = 26;
inline constexpr quint32 Xid = 28;
inline constexpr quint32 Json = 114;
inline constexpr quint32 Float4 = 700;
inline constexpr quint32 Float8 = 701;
inline constexpr quint32 Unknown = 705;
inline constexpr quint32 Bpchar = 1042;
inline constexpr quint32 Varchar = 1043;
inline constexpr quint32 Date = 1082;
inline constexpr quint32 Time = 1083;
inline constexpr quint32 Timestamp = 1114;
inline constexpr quint32 TimestampTz = 1184;
inline constexpr quint32 Interval = 1186;
inline constexpr quint32 TimeTz = 1266;
inline constexpr quint32 Numeric = 1700;
inline constexpr quint32 Uuid = 2950;
inline constexpr quint32 Jsonb = 3802;
inline constexpr quint32 Xid8 = 5069;
} // namespace oid

// What the driver knows about a type: its kind, display name and (for arrays) element type.
struct TypeInfo
{
    QString name; // "int4", "_text", "mood"
    ValueKind kind = ValueKind::Unknown;
    quint32 elementOid = 0; // arrays: the element type
};

// OID → TypeInfo. Starts with the built-in types; the connection adds the server's own
// (domains resolve to their base type, extension and user types) from pg_type.
class TypeRegistry
{
public:
    TypeRegistry();

    const TypeInfo *find(quint32 typeOid) const;
    void insert(quint32 typeOid, TypeInfo info) { m_types.insert(typeOid, std::move(info)); }
    qsizetype size() const { return m_types.size(); }

    // The value kind of a column / array element of this type.
    ValueKind kind(quint32 typeOid) const;
    // "numeric(10,2)", "varchar(255)", "timestamptz(3)", "int4[]"; empty if the type is unknown.
    QString displayName(quint32 typeOid, int typmod) const;

private:
    QHash<quint32, TypeInfo> m_types;
};

// Parses one non-NULL cell in text format. `previewLimit` bytes cap text and bytea (§6.3).
Value parseValue(QByteArrayView text, quint32 typeOid, const TypeRegistry &types,
                 qsizetype previewLimit);

// The individual parsers, exposed for tests. They keep `original` and fall back gracefully:
// text that does not parse yields a value with invalid parts but the original text.
Value parseDate(QByteArrayView text);
Value parseTime(QByteArrayView text, bool withZone);
Value parseTimestamp(QByteArrayView text, bool withZone);
Value parseInterval(QByteArrayView text);
Value parseArray(QByteArrayView text, quint32 elementOid, const TypeRegistry &types,
                 qsizetype previewLimit);

// A bound parameter in text format; std::nullopt for NULL. Temporal values use their original
// text when they have one, otherwise ISO text built from their parts; arrays become array literals.
std::optional<QByteArray> toParamText(const Value &value);

// Typmod decoding for ColumnInfo (PQfmod).
struct TypeModifiers
{
    int precision = -1;
    int scale = -1;
    int length = -1;
};
TypeModifiers decodeTypmod(quint32 typeOid, int typmod);

} // namespace owl::postgres
