// SPDX-License-Identifier: GPL-3.0-or-later
// The fake driver as a plugin built for another core API: must be rejected unloaded.
#include "fakedriver.h"

#include <QObject>

class FakeDriverPlugin
    : public QObject
    , public owl::test::FakeDriver
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OWL_DRIVER_IID FILE "fakeplugin_badapi.json")
    Q_INTERFACES(owl::IDriver)
};

#include "fakeplugin_badapi.moc"
