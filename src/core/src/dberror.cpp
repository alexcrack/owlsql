// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/dberror.h"

#include "cborutil.h"

#include <QCborMap>

using namespace Qt::StringLiterals;

namespace owl {

QString DbError::categoryName(Category category)
{
    switch (category) {
    case Category::Other:
        return QStringLiteral("Error");
    case Category::Connection:
        return QStringLiteral("Connection");
    case Category::Auth:
        return QStringLiteral("Authentication");
    case Category::Syntax:
        return QStringLiteral("Syntax");
    case Category::Constraint:
        return QStringLiteral("Constraint");
    case Category::Permission:
        return QStringLiteral("Permission");
    case Category::Timeout:
        return QStringLiteral("Timeout");
    case Category::Cancelled:
        return QStringLiteral("Cancelled");
    case Category::Lost:
        return QStringLiteral("Connection lost");
    case Category::Unsupported:
        return QStringLiteral("Unsupported");
    }
    return QStringLiteral("Error");
}

QString DbError::toString() const
{
    QString text = categoryName(category) + QStringLiteral(": ") + message;
    if (!sqlState.isEmpty())
        text += QStringLiteral(" (") + sqlState + QLatin1Char(')');
    return text;
}

QCborValue DbError::toCbor() const
{
    QCborMap m;
    m.insert(u"c"_s, static_cast<int>(category));
    m.insert(u"st"_s, sqlState);
    m.insert(u"nc"_s, nativeCode);
    m.insert(u"m"_s, message);
    m.insert(u"d"_s, detail);
    m.insert(u"h"_s, hint);
    m.insert(u"p"_s, position);
    return m;
}

std::optional<DbError> DbError::fromCbor(const QCborValue &cbor)
{
    if (!cbor.isMap())
        return std::nullopt;
    const QCborMap m = cbor.toMap();
    DbError e;
    qint64 category = 0, nativeCode = 0, position = -1;
    const bool ok = cbor::readInt(m, u"c"_s, category) && category >= 0
        && category <= static_cast<int>(Category::Unsupported)
        && cbor::readString(m, u"st"_s, e.sqlState) && cbor::readInt(m, u"nc"_s, nativeCode)
        && cbor::readString(m, u"m"_s, e.message) && cbor::readString(m, u"d"_s, e.detail)
        && cbor::readString(m, u"h"_s, e.hint) && cbor::readInt(m, u"p"_s, position);
    if (!ok)
        return std::nullopt;
    e.category = static_cast<Category>(category);
    e.nativeCode = static_cast<int>(nativeCode);
    e.position = static_cast<int>(position);
    return e;
}

QCborValue ServerMessage::toCbor() const
{
    QCborMap m;
    m.insert(u"sv"_s, static_cast<int>(severity));
    m.insert(u"m"_s, text);
    m.insert(u"st"_s, sqlState);
    return m;
}

std::optional<ServerMessage> ServerMessage::fromCbor(const QCborValue &cbor)
{
    if (!cbor.isMap())
        return std::nullopt;
    const QCborMap m = cbor.toMap();
    ServerMessage s;
    qint64 severity = static_cast<int>(s.severity);
    const bool ok = cbor::readInt(m, u"sv"_s, severity) && severity >= 0
        && severity <= static_cast<int>(Severity::Warning) && cbor::readString(m, u"m"_s, s.text)
        && cbor::readString(m, u"st"_s, s.sqlState);
    if (!ok)
        return std::nullopt;
    s.severity = static_cast<Severity>(severity);
    return s;
}

} // namespace owl
