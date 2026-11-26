// SPDX-License-Identifier: GPL-3.0-or-later
#include "uistate.h"

#include "appcontext.h"
#include "appdatabase.h"

#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>

Q_STATIC_LOGGING_CATEGORY(lcUiState, "owlsql.uistate")

namespace owl::app {

namespace {
// Values are stored as a one-element JSON array so scalars round-trip too.
QString encode(const QVariant &value)
{
    return QString::fromUtf8(
        QJsonDocument(QJsonArray{QJsonValue::fromVariant(value)}).toJson(QJsonDocument::Compact));
}

QVariant decode(const QString &text)
{
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    return doc.isArray() && !doc.array().isEmpty() ? doc.array().first().toVariant() : QVariant();
}
} // namespace

UiState::UiState(AppDatabase *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(500);
    connect(&m_flushTimer, &QTimer::timeout, this, &UiState::flush);
    load();
}

UiState::~UiState()
{
    flush();
}

UiState *UiState::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    UiState *state = AppContext::current()->uiState();
    QJSEngine::setObjectOwnership(state, QJSEngine::CppOwnership);
    return state;
}

void UiState::load()
{
    if (!m_database || !m_database->isOpen())
        return;
    QSqlQuery query(m_database->database());
    if (!query.exec(QStringLiteral("SELECT key, value FROM ui_state"))) {
        qCWarning(lcUiState) << "Cannot load UI state:" << query.lastError().text();
        return;
    }
    while (query.next())
        m_values.insert(query.value(0).toString(), decode(query.value(1).toString()));
}

QVariant UiState::value(const QString &key, const QVariant &defaultValue) const
{
    return m_values.value(key, defaultValue);
}

void UiState::setValue(const QString &key, const QVariant &value)
{
    // QML hands over JS values; normalise to plain JSON-compatible variants.
    const QVariant normalised = QJsonValue::fromVariant(value).toVariant();
    if (m_values.value(key) == normalised && m_values.contains(key))
        return;
    m_values.insert(key, normalised);
    m_dirty.insert(key);
    m_flushTimer.start();
}

void UiState::flush()
{
    m_flushTimer.stop();
    if (m_dirty.isEmpty() || !m_database || !m_database->isOpen()) {
        m_dirty.clear();
        return;
    }
    QSqlDatabase db = m_database->database();
    db.transaction();
    QSqlQuery query(db);
    query.prepare(QStringLiteral("INSERT INTO ui_state(key, value) VALUES (?, ?) "
                                 "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    for (const QString &key : std::as_const(m_dirty)) {
        query.addBindValue(key);
        query.addBindValue(encode(m_values.value(key)));
        if (!query.exec())
            qCWarning(lcUiState) << "Cannot save UI state" << key << query.lastError().text();
    }
    db.commit();
    m_dirty.clear();
}

} // namespace owl::app
