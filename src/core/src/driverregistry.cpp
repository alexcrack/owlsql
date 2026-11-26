// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/driverregistry.h"

#include <QDir>
#include <QJsonObject>
#include <QLibrary>
#include <QPluginLoader>

namespace owl {

DriverRegistry::DriverRegistry() = default;

DriverRegistry::~DriverRegistry()
{
    // Driver objects from plugins belong to their loader's root instance: drop our references
    // before the libraries could go away.
    m_byId.clear();
    m_drivers.clear();
    qDeleteAll(m_loaders);
}

bool DriverRegistry::add(std::shared_ptr<IDriver> driver)
{
    if (!driver || m_byId.contains(driver->id()))
        return false;
    m_byId.insert(driver->id(), driver.get());
    m_drivers.append(std::move(driver));
    return true;
}

QStringList DriverRegistry::loadPlugins(const QString &directory)
{
    QStringList problems;
    const QDir dir(directory);
    const QStringList files = dir.entryList(QDir::Files, QDir::Name);
    for (const QString &file : files) {
        const QString path = dir.absoluteFilePath(file);
        if (!QLibrary::isLibrary(path))
            continue;

        // Read the metadata first: a plugin built for another core API is never loaded.
        auto *loader = new QPluginLoader(path);
        const QJsonObject meta = loader->metaData();
        if (meta.value(QLatin1String("IID")).toString() != QLatin1String(OWL_DRIVER_IID)) {
            delete loader;
            continue; // not an OwlSql driver
        }
        const QJsonObject info = meta.value(QLatin1String("MetaData")).toObject();
        const int api = info.value(QLatin1String("coreApi")).toInt(-1);
        if (api != coreApiVersion) {
            problems << QStringLiteral("%1: built for core API %2, this OwlSql needs %3")
                            .arg(file)
                            .arg(api)
                            .arg(coreApiVersion);
            delete loader;
            continue;
        }

        QObject *root = loader->instance();
        auto *driver = qobject_cast<IDriver *>(root);
        if (!driver) {
            problems << QStringLiteral("%1: %2").arg(file, loader->errorString());
            delete loader;
            continue;
        }
        if (m_byId.contains(driver->id())) {
            problems << QStringLiteral("%1: driver id \"%2\" is already registered")
                            .arg(file, driver->id());
            loader->unload();
            delete loader;
            continue;
        }
        // The plugin's root object owns the driver; keep a non-owning shared_ptr.
        m_byId.insert(driver->id(), driver);
        m_drivers.append(std::shared_ptr<IDriver>(driver, [](IDriver *) { }));
        m_loaders.append(loader);
    }
    return problems;
}

IDriver *DriverRegistry::driver(const QString &id) const
{
    return m_byId.value(id);
}

QList<IDriver *> DriverRegistry::drivers() const
{
    QList<IDriver *> list;
    list.reserve(m_drivers.size());
    for (const auto &d : m_drivers)
        list.append(d.get());
    return list;
}

QStringList DriverRegistry::ids() const
{
    QStringList list;
    for (const auto &d : m_drivers)
        list << d->id();
    return list;
}

} // namespace owl
