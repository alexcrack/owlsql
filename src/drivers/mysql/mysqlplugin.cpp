// SPDX-License-Identifier: GPL-3.0-or-later
#include "mysqldriver.h"

#include <QObject>

class MySqlDriverPlugin
    : public QObject
    , public owl::mysql::MySqlDriver
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OWL_DRIVER_IID FILE "mysql.json")
    Q_INTERFACES(owl::IDriver)
};

#include "mysqlplugin.moc"
