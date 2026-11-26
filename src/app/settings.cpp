// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"

#include "appcontext.h"
#include "watchedfile.h"

#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>

Q_STATIC_LOGGING_CATEGORY(lcSettings, "owlsql.settings")

namespace owl::app {

QList<Settings::Key> Settings::schema()
{
    return {
        {.name = QStringLiteral("theme.mode"),
         .type = QMetaType::QString,
         .defaultValue = QStringLiteral("system"),
         .allowed = {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")},
         .description = QStringLiteral("Colour theme: follow the system, or always light/dark.")},
        {.name = QStringLiteral("ui.reduceMotion"),
         .type = QMetaType::Bool,
         .defaultValue = false,
         .allowed = {},
         .description = QStringLiteral("Turn off UI animations.")},
    };
}

Settings::Settings(const QString &filePath, QObject *parent)
    : QObject(parent)
    , m_schema(schema())
    , m_file(new WatchedFile(filePath, this))
{
    connect(m_file, &WatchedFile::changedExternally, this, &Settings::reload);
    reload();
}

Settings::~Settings() = default;

Settings *Settings::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    Settings *settings = AppContext::current()->settings();
    QJSEngine::setObjectOwnership(settings, QJSEngine::CppOwnership);
    return settings;
}

QString Settings::filePath() const
{
    return m_file->path();
}

const Settings::Key *Settings::findKey(const QString &name) const
{
    for (const Key &key : m_schema) {
        if (key.name == name)
            return &key;
    }
    return nullptr;
}

std::optional<QVariant> Settings::validate(const Key &key, const QJsonValue &value) const
{
    switch (key.type) {
    case QMetaType::Bool:
        if (value.isBool())
            return value.toBool();
        break;
    case QMetaType::Int:
        if (value.isDouble() && value.toDouble() == static_cast<double>(value.toInt()))
            return value.toInt();
        break;
    case QMetaType::QString:
        if (value.isString() && (key.allowed.isEmpty() || key.allowed.contains(value.toString())))
            return value.toString();
        break;
    default:
        break;
    }
    return std::nullopt;
}

void Settings::reload()
{
    QStringList problems;
    m_raw = {};
    if (m_file->exists()) {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(m_file->read(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            problems << tr("settings.json is not a valid JSON object (%1); using defaults.")
                            .arg(parseError.errorString());
        } else {
            m_raw = doc.object();
        }
    }

    QHash<QString, QVariant> values;
    for (const Key &key : std::as_const(m_schema)) {
        values.insert(key.name, key.defaultValue);
        if (!m_raw.contains(key.name))
            continue;
        if (const auto valid = validate(key, m_raw.value(key.name))) {
            values.insert(key.name, *valid);
        } else {
            problems << tr("%1: invalid value %2; using the default %3.")
                            .arg(key.name,
                                 QString::fromUtf8(QJsonDocument(QJsonArray{m_raw.value(key.name)})
                                                       .toJson(QJsonDocument::Compact))
                                     .mid(1)
                                     .chopped(1),
                                 key.defaultValue.toString());
        }
    }
    for (auto it = m_raw.constBegin(); it != m_raw.constEnd(); ++it) {
        if (!findKey(it.key()))
            problems << tr("%1: unknown setting (kept as is).").arg(it.key());
    }

    const QHash<QString, QVariant> old = std::exchange(m_values, values);
    for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        if (old.value(it.key()) != it.value())
            emit valueChanged(it.key(), it.value());
    }
    if (problems != m_problems) {
        m_problems = problems;
        for (const QString &problem : std::as_const(m_problems))
            qCWarning(lcSettings).noquote() << problem;
        emit problemsChanged();
    }
}

QVariant Settings::value(const QString &key) const
{
    if (!findKey(key))
        qCWarning(lcSettings) << "Unknown setting" << key;
    return m_values.value(key);
}

bool Settings::setValue(const QString &key, const QVariant &value)
{
    const Key *spec = findKey(key);
    if (!spec) {
        qCWarning(lcSettings) << "Unknown setting" << key;
        return false;
    }
    const auto valid = validate(*spec, QJsonValue::fromVariant(value));
    if (!valid) {
        qCWarning(lcSettings) << "Invalid value for" << key << value;
        return false;
    }
    if (*valid == spec->defaultValue)
        m_raw.remove(key);
    else
        m_raw.insert(key, QJsonValue::fromVariant(*valid));
    save();

    if (m_values.value(key) != *valid) {
        m_values.insert(key, *valid);
        emit valueChanged(key, *valid);
    }
    return true;
}

void Settings::reset(const QString &key)
{
    if (const Key *spec = findKey(key))
        setValue(key, spec->defaultValue);
}

void Settings::save()
{
    QString error;
    const QByteArray contents = m_raw.isEmpty()
        ? QByteArray("{\n}\n")
        : QJsonDocument(m_raw).toJson(QJsonDocument::Indented);
    if (!m_file->write(contents, &error))
        qCWarning(lcSettings).noquote() << error;
}

} // namespace owl::app
