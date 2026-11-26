// SPDX-License-Identifier: GPL-3.0-or-later
#include "pgdriver.h"

#include <QObject>

class PgDriverPlugin
    : public QObject
    , public owl::postgres::PgDriver
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OWL_DRIVER_IID FILE "postgres.json")
    Q_INTERFACES(owl::IDriver)
};

#include "pgplugin.moc"
