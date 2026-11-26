// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>

#include <optional>

class QJSEngine;
class QQmlEngine;

namespace owl::app {

class WatchedFile;

// User preferences in a human-editable settings.json (docs/specs/app-store.md §5).
// Defaults are compiled in (schema()); the file holds only non-default values. Unknown keys are
// kept, invalid values fall back to defaults and are reported in problems, edits apply live.
// QML: Settings.value("theme.mode"); Settings.setValue("theme.mode", "dark")
class Settings : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Settings)
    QML_SINGLETON

    Q_PROPERTY(QStringList problems READ problems NOTIFY problemsChanged FINAL)

public:
    struct Key
    {
        QString name;
        QMetaType::Type type = QMetaType::UnknownType; // Bool, Int or QString
        QVariant defaultValue;
        QStringList allowed; // for strings: allowed values (empty = any)
        QString description;
    };

    Settings(const QString &filePath, QObject *parent);
    ~Settings() override;

    static Settings *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static QList<Key> schema();

    Q_INVOKABLE QVariant value(const QString &key) const;
    // Validates and saves; returns false (and changes nothing) for unknown keys/invalid values.
    Q_INVOKABLE bool setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void reset(const QString &key);

    QStringList problems() const { return m_problems; }
    QString filePath() const;
    void reload();

signals:
    void valueChanged(const QString &key, const QVariant &value);
    void problemsChanged();

private:
    const Key *findKey(const QString &name) const;
    std::optional<QVariant> validate(const Key &key, const QJsonValue &value) const;
    void save();

    QList<Key> m_schema;
    WatchedFile *m_file;
    QJsonObject m_raw; // file contents (unknown keys preserved)
    QHash<QString, QVariant> m_values; // effective values for every schema key
    QStringList m_problems;
};

} // namespace owl::app
