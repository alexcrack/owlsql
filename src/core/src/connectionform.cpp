// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/connectionform.h"

#include "owl/core/driver.h"

#include <QJsonArray>

#include <array>

namespace owl {

namespace {

constexpr QLatin1StringView optionPrefix("option.");

using Mode = SslOptions::Mode;
constexpr std::array<std::pair<Mode, const char *>, 5> sslModes{{
    {Mode::Disable, "disable"},
    {Mode::Prefer, "prefer"},
    {Mode::Require, "require"},
    {Mode::VerifyCa, "verify-ca"},
    {Mode::VerifyFull, "verify-full"},
}};

QVariant get(const ConnectionParams &p, const QString &key)
{
    if (key.startsWith(optionPrefix))
        return p.driverOptions.value(key.mid(optionPrefix.size()));
    if (key == u"transport")
        return p.transport == ConnectionParams::Transport::Socket ? QStringLiteral("socket")
                                                                  : QStringLiteral("tcp");
    if (key == u"host")
        return p.host;
    if (key == u"port")
        return p.port == 0 ? QVariant() : QVariant(int{p.port});
    if (key == u"socketPath")
        return p.socketPath;
    if (key == u"user")
        return p.user;
    if (key == u"password")
        return p.password;
    if (key == u"database")
        return p.database;
    if (key == u"ssl.mode")
        return sslModeName(p.ssl.mode);
    if (key == u"ssl.caFile")
        return p.ssl.caFile;
    if (key == u"ssl.certFile")
        return p.ssl.certFile;
    if (key == u"ssl.keyFile")
        return p.ssl.keyFile;
    if (key == u"applicationName")
        return p.applicationName;
    if (key == u"connectTimeout")
        return static_cast<int>(p.connectTimeout.count());
    return {};
}

void set(ConnectionParams &p, const QString &key, const QVariant &v)
{
    if (key.startsWith(optionPrefix)) {
        if (v.isValid() && (v.typeId() != QMetaType::QString || !v.toString().isEmpty()))
            p.driverOptions.insert(key.mid(optionPrefix.size()), v);
        return;
    }
    const QString s = v.toString();
    if (key == u"transport")
        p.transport = s == u"socket" ? ConnectionParams::Transport::Socket
                                     : ConnectionParams::Transport::Tcp;
    else if (key == u"host")
        p.host = s;
    else if (key == u"port")
        p.port = static_cast<quint16>(v.toInt());
    else if (key == u"socketPath")
        p.socketPath = s;
    else if (key == u"user")
        p.user = s;
    else if (key == u"password")
        p.password = s;
    else if (key == u"database")
        p.database = s;
    else if (key == u"ssl.mode")
        p.ssl.mode = sslModeFromName(s).value_or(Mode::Prefer);
    else if (key == u"ssl.caFile")
        p.ssl.caFile = s;
    else if (key == u"ssl.certFile")
        p.ssl.certFile = s;
    else if (key == u"ssl.keyFile")
        p.ssl.keyFile = s;
    else if (key == u"applicationName")
        p.applicationName = s;
    else if (key == u"connectTimeout")
        p.connectTimeout = std::chrono::seconds(qMax(1, v.toInt()));
}

bool isEmpty(const QVariant &v)
{
    return !v.isValid() || v.isNull() || v.toString().trimmed().isEmpty();
}

bool visible(const FormField &f, const QVariantMap &values, const ConnectionFormSchema &schema)
{
    if (f.visibleWhenKey.isEmpty())
        return true;
    QVariant v = values.value(f.visibleWhenKey);
    if (!v.isValid()) {
        if (const FormField *other = schema.field(f.visibleWhenKey))
            v = other->defaultValue;
    }
    return v.toString() == f.visibleWhenValue.toString();
}

} // namespace

QString sslModeName(SslOptions::Mode mode)
{
    for (const auto &[m, name] : sslModes) {
        if (m == mode)
            return QString::fromLatin1(name);
    }
    return QStringLiteral("prefer");
}

std::optional<SslOptions::Mode> sslModeFromName(QStringView name)
{
    for (const auto &[m, n] : sslModes) {
        if (name.compare(QLatin1StringView(n), Qt::CaseInsensitive) == 0)
            return m;
    }
    // MySQL's spellings (--ssl-mode): DISABLED, PREFERRED, REQUIRED, VERIFY_CA, VERIFY_IDENTITY.
    if (name.compare(u"disabled", Qt::CaseInsensitive) == 0)
        return Mode::Disable;
    if (name.compare(u"preferred", Qt::CaseInsensitive) == 0)
        return Mode::Prefer;
    if (name.compare(u"required", Qt::CaseInsensitive) == 0)
        return Mode::Require;
    if (name.compare(u"verify_ca", Qt::CaseInsensitive) == 0)
        return Mode::VerifyCa;
    if (name.compare(u"verify_identity", Qt::CaseInsensitive) == 0)
        return Mode::VerifyFull;
    return std::nullopt;
}

const FormField *ConnectionFormSchema::field(QStringView key) const
{
    for (const FormField &f : fields) {
        if (f.key == key)
            return &f;
    }
    return nullptr;
}

QVariantMap formValues(const ConnectionFormSchema &schema, const ConnectionParams &params)
{
    QVariantMap out;
    for (const FormField &f : schema.fields) {
        QVariant v = get(params, f.key);
        if (isEmpty(v) && f.type != FormField::Type::Bool)
            v = f.defaultValue;
        if (f.type == FormField::Type::Bool && !v.isValid())
            v = f.defaultValue.isValid() ? f.defaultValue : QVariant(false);
        out.insert(f.key, v);
    }
    return out;
}

Result<ConnectionParams> paramsFromForm(const ConnectionFormSchema &schema, const QString &driverId,
                                        const QVariantMap &values, QList<FieldError> *errors)
{
    ConnectionParams p;
    p.driverId = driverId;
    QList<FieldError> problems;
    for (const FormField &f : schema.fields) {
        if (!visible(f, values, schema))
            continue;
        QVariant v = values.contains(f.key) ? values.value(f.key) : f.defaultValue;
        if (f.required && isEmpty(v)) {
            problems.append(
                {.key = f.key, .message = QStringLiteral("%1 is required").arg(f.label)});
            continue;
        }
        switch (f.type) {
        case FormField::Type::Integer: {
            if (isEmpty(v))
                break;
            bool ok = false;
            const int n = v.toString().trimmed().toInt(&ok);
            if (!ok || n < f.minimum || (f.maximum > 0 && n > f.maximum)) {
                problems.append({.key = f.key,
                                 .message = QStringLiteral("%1 must be a number from %2 to %3")
                                                .arg(f.label)
                                                .arg(f.minimum)
                                                .arg(f.maximum)});
                continue;
            }
            v = n;
            break;
        }
        case FormField::Type::Bool:
            v = v.toBool();
            break;
        case FormField::Type::Choice: {
            const QString s = v.toString();
            const bool offered
                = std::ranges::any_of(f.choices, [&](const FormChoice &c) { return c.value == s; });
            if (!offered && !s.isEmpty()) {
                problems.append(
                    {.key = f.key,
                     .message = QStringLiteral("%1: \"%2\" is not an option").arg(f.label, s)});
                continue;
            }
            break;
        }
        default:
            if (v.typeId() == QMetaType::QString)
                v = v.toString().trimmed();
            break;
        }
        set(p, f.key, v);
    }
    if (errors)
        *errors = problems;
    if (!problems.isEmpty()) {
        DbError e;
        e.message = problems.first().message;
        return fail(e);
    }
    return p;
}

QJsonObject toJson(const ConnectionParams &p)
{
    QJsonObject o;
    o.insert(QStringLiteral("transport"),
             p.transport == ConnectionParams::Transport::Socket ? QStringLiteral("socket")
                                                                : QStringLiteral("tcp"));
    const auto put = [&o](const char *key, const QString &value) {
        if (!value.isEmpty())
            o.insert(QLatin1StringView(key), value);
    };
    put("host", p.host);
    if (p.port != 0)
        o.insert(QStringLiteral("port"), int{p.port});
    put("socketPath", p.socketPath);
    put("user", p.user);
    put("database", p.database);
    QJsonObject ssl;
    ssl.insert(QStringLiteral("mode"), sslModeName(p.ssl.mode));
    if (!p.ssl.caFile.isEmpty())
        ssl.insert(QStringLiteral("caFile"), p.ssl.caFile);
    if (!p.ssl.certFile.isEmpty())
        ssl.insert(QStringLiteral("certFile"), p.ssl.certFile);
    if (!p.ssl.keyFile.isEmpty())
        ssl.insert(QStringLiteral("keyFile"), p.ssl.keyFile);
    o.insert(QStringLiteral("ssl"), ssl);
    if (!p.driverOptions.isEmpty())
        o.insert(QStringLiteral("options"), QJsonObject::fromVariantMap(p.driverOptions));
    if (p.applicationName != u"OwlSql")
        o.insert(QStringLiteral("applicationName"), p.applicationName);
    if (p.connectTimeout != std::chrono::seconds(15))
        o.insert(QStringLiteral("connectTimeout"), static_cast<int>(p.connectTimeout.count()));
    return o;
}

ConnectionParams connectionParamsFromJson(const QJsonObject &o, const QString &driverId)
{
    ConnectionParams p;
    p.driverId = driverId;
    p.transport = o.value(u"transport").toString() == u"socket"
        ? ConnectionParams::Transport::Socket
        : ConnectionParams::Transport::Tcp;
    p.host = o.value(u"host").toString();
    p.port = static_cast<quint16>(o.value(u"port").toInt());
    p.socketPath = o.value(u"socketPath").toString();
    p.user = o.value(u"user").toString();
    p.database = o.value(u"database").toString();
    const QJsonObject ssl = o.value(u"ssl").toObject();
    p.ssl.mode = sslModeFromName(ssl.value(u"mode").toString()).value_or(SslOptions::Mode::Prefer);
    p.ssl.caFile = ssl.value(u"caFile").toString();
    p.ssl.certFile = ssl.value(u"certFile").toString();
    p.ssl.keyFile = ssl.value(u"keyFile").toString();
    p.driverOptions = o.value(u"options").toObject().toVariantMap();
    if (o.contains(u"applicationName"))
        p.applicationName = o.value(u"applicationName").toString();
    if (o.contains(u"connectTimeout"))
        p.connectTimeout = std::chrono::seconds(qMax(1, o.value(u"connectTimeout").toInt()));
    return p;
}

} // namespace owl
