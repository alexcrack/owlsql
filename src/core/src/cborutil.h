// SPDX-License-Identifier: GPL-3.0-or-later
// Internal helpers for decoding CBOR maps: a missing key keeps the current (default) value,
// a key with the wrong type fails the decode.
#pragma once

#include <QCborMap>
#include <QString>

namespace owl::cbor {

inline bool readString(const QCborMap &m, const QString &key, QString &out)
{
    const QCborValue v = m.value(key);
    if (v.isUndefined())
        return true;
    if (!v.isString())
        return false;
    out = v.toString();
    return true;
}

inline bool readInt(const QCborMap &m, const QString &key, qint64 &out)
{
    const QCborValue v = m.value(key);
    if (v.isUndefined())
        return true;
    if (!v.isInteger())
        return false;
    out = v.toInteger();
    return true;
}

inline bool readBool(const QCborMap &m, const QString &key, bool &out)
{
    const QCborValue v = m.value(key);
    if (v.isUndefined())
        return true;
    if (!v.isBool())
        return false;
    out = v.toBool();
    return true;
}

} // namespace owl::cbor
