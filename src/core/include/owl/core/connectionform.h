// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/result.h"

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

// Connection forms (driver-api.md §13, decision 166): each driver describes the fields of its
// connection dialog; the UI renders them generically, and values map to ConnectionParams.

namespace owl {

struct ConnectionParams;

struct FormChoice
{
    QString value; // stored value ("prefer")
    QString label; // shown ("Prefer TLS")

    bool operator==(const FormChoice &) const = default;
};

struct FormField
{
    enum class Type : quint8 {
        Text,
        Password, // never stored with the session: goes to the keychain (app-store.md §2)
        Integer,
        Bool,
        Choice,
        File, // a file path (SSL certificates, SQLite database)
        Directory,
        Multiline,
    };

    // Where the value lives: a ConnectionParams field — "transport", "host", "port",
    // "socketPath", "user", "password", "database", "ssl.mode", "ssl.caFile", "ssl.certFile",
    // "ssl.keyFile", "applicationName", "connectTimeout" — or "option.<name>" for
    // ConnectionParams::driverOptions.
    QString key;
    Type type = Type::Text;
    QString label;
    QString group; // "connection", "authentication", "ssl", "advanced" — UI sections
    QVariant defaultValue;
    QList<FormChoice> choices; // Choice
    bool required = false;
    int minimum = 0; // Integer
    int maximum = 0; // Integer; 0 = no limit
    QString placeholder;
    QString help;
    // Shown (and validated) only when the field `visibleWhenKey` has value `visibleWhenValue`.
    QString visibleWhenKey;
    QVariant visibleWhenValue;

    bool operator==(const FormField &) const = default;
};

struct ConnectionFormSchema
{
    QList<FormField> fields;

    const FormField *field(QStringView key) const;
};

struct FieldError
{
    QString key;
    QString message;

    bool operator==(const FieldError &) const = default;
};

// The form's values for `params` (missing ones: field defaults).
QVariantMap formValues(const ConnectionFormSchema &schema, const ConnectionParams &params);
// The ConnectionParams for form `values` (unknown keys ignored; hidden fields skipped), or the
// list of problems: missing required fields, integers out of range, choices not offered.
Result<ConnectionParams> paramsFromForm(const ConnectionFormSchema &schema, const QString &driverId,
                                        const QVariantMap &values,
                                        QList<FieldError> *errors = nullptr);

// ConnectionParams as JSON for storage and export (app-store.md §3, §6). The password is never
// written; unknown keys are ignored when reading.
QJsonObject toJson(const ConnectionParams &params);
ConnectionParams connectionParamsFromJson(const QJsonObject &json, const QString &driverId);

} // namespace owl
