// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>

class QJSEngine;
class QQmlEngine;

namespace owl::app {

class AppDatabase;

// Remembered UI state (window size, splitter sizes, open tabs...) in owlsql.db's ui_state table.
// QML: UiState.value("layout.sidebarWidth", 260); UiState.setValue("layout.sidebarWidth", w)
// Values must be JSON-compatible. Writes are batched and flushed on quit.
class UiState : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(UiState)
    QML_SINGLETON

public:
    // database may be null: values then live in memory only.
    UiState(AppDatabase *database, QObject *parent);
    ~UiState() override;

    static UiState *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = {}) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);

    // Writes pending changes now (also done automatically shortly after changes and on quit).
    void flush();

private:
    void load();

    AppDatabase *m_database;
    QHash<QString, QVariant> m_values;
    QSet<QString> m_dirty;
    QTimer m_flushTimer;
};

} // namespace owl::app
