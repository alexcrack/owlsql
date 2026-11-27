// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/catalogcache.h"

#include "owl/core/connectionworker.h"

#include <QPointer>

namespace owl {

CatalogCache::CatalogCache(ConnectionWorker *worker, QObject *parent)
    : QObject(parent)
    , m_worker(worker)
{ }

std::optional<QList<ObjectNode>> CatalogCache::children(const ObjectPath &path) const
{
    const auto it = m_children.constFind(path);
    return it == m_children.cend() ? std::nullopt : std::optional(*it);
}

std::optional<TableDef> CatalogCache::table(const ObjectPath &path) const
{
    const auto it = m_tables.constFind(path);
    return it == m_tables.cend() ? std::nullopt : std::optional(*it);
}

std::optional<QString> CatalogCache::ddl(const ObjectPath &path) const
{
    const auto it = m_ddl.constFind(path);
    return it == m_ddl.cend() ? std::nullopt : std::optional(*it);
}

bool CatalogCache::isLoading(What what, const ObjectPath &path) const
{
    return m_loading.contains(Key{.what = what, .path = path});
}

void CatalogCache::load(What what, const ObjectPath &path)
{
    Key key{.what = what, .path = path};
    if (m_loading.contains(key))
        return;
    const quint64 token = m_nextToken++;
    m_loading.insert(key, token);

    // The answer is posted to the worker, which lives on this thread and outlives its own
    // thread; the QPointer drops it if the cache is gone by then (decision 139).
    QPointer<CatalogCache> self(this);
    ConnectionWorker *worker = m_worker;
    m_worker->runTask([self, worker, key, token](IConnection *connection) {
        Result<Payload> result = [&]() -> Result<Payload> {
            if (!connection)
                return fail(DbError::Category::Connection, QStringLiteral("Not connected"));
            ICatalog &catalog = connection->catalog();
            switch (key.what) {
            case What::Children:
                if (auto r = catalog.children(key.path))
                    return Payload(std::move(*r));
                else
                    return fail(r.error());
            case What::Table:
                if (auto r = catalog.table(key.path))
                    return Payload(std::move(*r));
                else
                    return fail(r.error());
            case What::Ddl:
                if (auto r = catalog.ddl(key.path))
                    return Payload(std::move(*r));
                else
                    return fail(r.error());
            }
            return fail(DbError::Category::Other, QStringLiteral("Unknown catalog request"));
        }();
        QMetaObject::invokeMethod(
            worker,
            [self, key, token, result = std::move(result)]() mutable {
                if (self)
                    self->deliver(key, token, std::move(result));
            },
            Qt::QueuedConnection);
    });
}

void CatalogCache::deliver(const Key &key, quint64 token, Result<Payload> result)
{
    const auto it = m_loading.constFind(key);
    if (it == m_loading.cend() || *it != token)
        return; // invalidated while loading: a newer load (or none) is wanted
    m_loading.erase(it);
    if (!result) {
        emit loadFailed(key.what, key.path, result.error());
        return;
    }
    switch (key.what) {
    case What::Children:
        m_children.insert(key.path, std::get<QList<ObjectNode>>(std::move(*result)));
        break;
    case What::Table:
        m_tables.insert(key.path, std::get<TableDef>(std::move(*result)));
        break;
    case What::Ddl:
        m_ddl.insert(key.path, std::get<QString>(std::move(*result)));
        break;
    }
    emit loaded(key.what, key.path);
}

void CatalogCache::invalidate(const ObjectPath &path)
{
    const auto below = [&path](const ObjectPath &p) {
        return p.startsWith(path);
    };
    m_children.removeIf([&](const auto &it) { return below(it.key()); });
    m_tables.removeIf([&](const auto &it) { return below(it.key()); });
    m_ddl.removeIf([&](const auto &it) { return below(it.key()); });
    m_loading.removeIf([&](const auto &it) { return below(it.key().path); });
    emit invalidated(path);
}

void CatalogCache::noteExecuted(StatementClass statementClass)
{
    if (statementClass == StatementClass::Ddl)
        invalidate(ObjectPath{});
}

} // namespace owl
