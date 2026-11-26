// SPDX-License-Identifier: GPL-3.0-or-later
#include "sqlitedriver.h"

#include <QObject>

class SqliteDriverPlugin
    : public QObject
    , public owl::sqlite::SqliteDriver
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OWL_DRIVER_IID FILE "sqlite.json")
    Q_INTERFACES(owl::IDriver)
};

#include "sqliteplugin.moc"
