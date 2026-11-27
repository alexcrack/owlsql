// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/driver.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include <memory>

class QPluginLoader;

namespace owl {

// The available database drivers (decision 049): plugins loaded from directories, plus drivers
// added directly (tests, statically linked builds). Not thread-safe: use from the main thread.
class DriverRegistry
{
public:
    DriverRegistry();
    ~DriverRegistry();
    DriverRegistry(const DriverRegistry &) = delete;
    DriverRegistry &operator=(const DriverRegistry &) = delete;

    // Loads every driver plugin in `directory`. A plugin is only loaded when its metadata
    // declares our coreApiVersion; mismatches and failures are returned as readable problems.
    QStringList loadPlugins(const QString &directory);

    // Adds a driver that is not a plugin. Returns false if the id is already registered.
    bool add(std::shared_ptr<IDriver> driver);

    IDriver *driver(const QString &id) const;
    QList<IDriver *> drivers() const; // in registration order
    QStringList ids() const;

    // The first driver that understands `url` (IDriver::parseUrl), with its ConnectionParams.
    // Unsupported when no driver does.
    Result<ConnectionParams> parseUrl(const QString &url) const;

private:
    QList<std::shared_ptr<IDriver>> m_drivers;
    QHash<QString, IDriver *> m_byId;
    QList<QPluginLoader *> m_loaders;
};

} // namespace owl
