// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/formbuilder.h"

namespace owl::form {

FormField text(const QString &key, const QString &label, const QString &group, bool required,
               const QVariant &defaultValue)
{
    FormField f;
    f.key = key;
    f.label = label;
    f.group = group;
    f.required = required;
    f.defaultValue = defaultValue;
    return f;
}

FormField integer(const QString &key, const QString &label, const QString &group,
                  const QVariant &defaultValue, int minimum, int maximum)
{
    FormField f = text(key, label, group, false, defaultValue);
    f.type = FormField::Type::Integer;
    f.minimum = minimum;
    f.maximum = maximum;
    return f;
}

FormField flag(const QString &key, const QString &label, const QString &group, bool defaultValue)
{
    FormField f = text(key, label, group, false, defaultValue);
    f.type = FormField::Type::Bool;
    return f;
}

FormField path(const QString &key, const QString &label, const QString &group, bool directory,
               bool required)
{
    FormField f = text(key, label, group, required);
    f.type = directory ? FormField::Type::Directory : FormField::Type::File;
    return f;
}

QList<FormField> serverFields(int defaultPort, const QString &defaultDatabase,
                              bool databaseRequired, bool socketIsDirectory,
                              const QString &defaultSocket)
{
    const QString connection = QStringLiteral("connection");
    const QString auth = QStringLiteral("authentication");
    const QString ssl = QStringLiteral("ssl");
    const QString advanced = QStringLiteral("advanced");
    QList<FormField> fields;

    FormField transport = text(QStringLiteral("transport"), QStringLiteral("Connect via"),
                               connection, true, QStringLiteral("tcp"));
    transport.type = FormField::Type::Choice;
    transport.choices
        = {{.value = QStringLiteral("tcp"), .label = QStringLiteral("TCP/IP")},
           {.value = QStringLiteral("socket"), .label = QStringLiteral("Unix socket")}};
    fields << transport;

    FormField host = text(QStringLiteral("host"), QStringLiteral("Host"), connection, true,
                          QStringLiteral("localhost"));
    host.visibleWhenKey = transport.key;
    host.visibleWhenValue = QStringLiteral("tcp");
    fields << host;

    FormField port = integer(QStringLiteral("port"), QStringLiteral("Port"), connection,
                             defaultPort, 1, 65535);
    port.visibleWhenKey = transport.key;
    port.visibleWhenValue = QStringLiteral("tcp");
    fields << port;

    FormField socket = path(QStringLiteral("socketPath"),
                            socketIsDirectory ? QStringLiteral("Socket directory")
                                              : QStringLiteral("Socket file"),
                            connection, socketIsDirectory, true);
    socket.defaultValue = defaultSocket;
    socket.visibleWhenKey = transport.key;
    socket.visibleWhenValue = QStringLiteral("socket");
    fields << socket;

    FormField database = text(QStringLiteral("database"), QStringLiteral("Database"), connection,
                              databaseRequired, defaultDatabase);
    fields << database;

    fields << text(QStringLiteral("user"), QStringLiteral("User"), auth, true);
    FormField password = text(QStringLiteral("password"), QStringLiteral("Password"), auth);
    password.type = FormField::Type::Password;
    fields << password;

    FormField mode = text(QStringLiteral("ssl.mode"), QStringLiteral("TLS / SSL"), ssl, true,
                          QStringLiteral("prefer"));
    mode.type = FormField::Type::Choice;
    mode.choices = {
        {.value = QStringLiteral("disable"), .label = QStringLiteral("Disable")},
        {.value = QStringLiteral("prefer"), .label = QStringLiteral("Prefer")},
        {.value = QStringLiteral("require"), .label = QStringLiteral("Require")},
        {.value = QStringLiteral("verify-ca"), .label = QStringLiteral("Verify CA")},
        {.value = QStringLiteral("verify-full"),
         .label = QStringLiteral("Verify CA and host name")},
    };
    fields << mode;
    fields << path(QStringLiteral("ssl.caFile"), QStringLiteral("CA certificate"), ssl, false);
    fields << path(QStringLiteral("ssl.certFile"), QStringLiteral("Client certificate"), ssl,
                   false);
    fields << path(QStringLiteral("ssl.keyFile"), QStringLiteral("Client key"), ssl, false);

    fields << integer(QStringLiteral("connectTimeout"), QStringLiteral("Connect timeout (s)"),
                      advanced, 15, 1, 600);
    return fields;
}

} // namespace owl::form
