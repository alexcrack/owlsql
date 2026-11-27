// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/catalog.h"
#include "owl/core/dialect.h"

#include <QHash>
#include <QObject>

#include <optional>

namespace owl {

class ConnectionWorker;

// Lazily loaded catalog of one connection (driver-api.md §4, decision 050), for the object tree,
// object tabs and completion. Lives on the main thread; loads run as tasks on the
// ConnectionWorker. A load of something that is invalidated while it runs is dropped.
class CatalogCache : public QObject
{
    Q_OBJECT

public:
    enum class What : quint8 { Children, Table, Ddl };
    Q_ENUM(What)

    // `worker` must outlive the cache.
    explicit CatalogCache(ConnectionWorker *worker, QObject *parent = nullptr);

    // Cached data, std::nullopt until loaded.
    std::optional<QList<ObjectNode>> children(const ObjectPath &path) const;
    std::optional<TableDef> table(const ObjectPath &path) const;
    std::optional<QString> ddl(const ObjectPath &path) const;
    bool isLoading(What what, const ObjectPath &path) const;

    // Starts a load; no-op while the same load is running. Loading what is cached reloads it.
    void loadChildren(const ObjectPath &path) { load(What::Children, path); }
    void loadTable(const ObjectPath &path) { load(What::Table, path); }
    void loadDdl(const ObjectPath &path) { load(What::Ddl, path); }

    // Forgets `path` and everything below it (F5 on a node; the root clears everything).
    void invalidate(const ObjectPath &path);
    // A statement OwlSql ran on this session: DDL may have changed anything, so the cache is
    // cleared (decision 162); expanded nodes reload as they are shown.
    void noteExecuted(StatementClass statementClass);

signals:
    void loaded(owl::CatalogCache::What what, const owl::ObjectPath &path);
    void loadFailed(owl::CatalogCache::What what, const owl::ObjectPath &path,
                    const owl::DbError &error);
    void invalidated(const owl::ObjectPath &path);

private:
    struct Key
    {
        What what;
        ObjectPath path;
        bool operator==(const Key &) const = default;
    };
    friend size_t qHash(const Key &key, size_t seed)
    {
        return qHashMulti(seed, key.what, key.path);
    }
    using Payload = std::variant<QList<ObjectNode>, TableDef, QString>;

    void load(What what, const ObjectPath &path);
    void deliver(const Key &key, quint64 token, Result<Payload> result);

    ConnectionWorker *m_worker;
    QHash<ObjectPath, QList<ObjectNode>> m_children;
    QHash<ObjectPath, TableDef> m_tables;
    QHash<ObjectPath, QString> m_ddl;
    QHash<Key, quint64> m_loading; // load → token of the request whose answer counts
    quint64 m_nextToken = 1;
};

} // namespace owl
