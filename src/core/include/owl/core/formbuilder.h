// SPDX-License-Identifier: GPL-3.0-or-later
// Building blocks for drivers' connection forms (connectionform.h): the fields most engines share.
#pragma once

#include "owl/core/connectionform.h"

namespace owl::form {

FormField text(const QString &key, const QString &label, const QString &group,
               bool required = false, const QVariant &defaultValue = {});
FormField integer(const QString &key, const QString &label, const QString &group,
                  const QVariant &defaultValue, int minimum, int maximum);
FormField flag(const QString &key, const QString &label, const QString &group, bool defaultValue);
FormField path(const QString &key, const QString &label, const QString &group, bool directory,
               bool required = false);

// Host / port or a Unix socket, user, password, database, SSL and timeouts — the server engines'
// common form. `socketIsDirectory`: PostgreSQL wants the socket directory, MySQL the file.
QList<FormField> serverFields(int defaultPort, const QString &defaultDatabase,
                              bool databaseRequired, bool socketIsDirectory,
                              const QString &defaultSocket);

} // namespace owl::form
