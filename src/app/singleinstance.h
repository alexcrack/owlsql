// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

namespace owl::app {

// One OwlSql process per user and data directory (docs/specs/app-store.md §9). A second launch
// forwards its arguments to the running instance, which raises its window, and then exits.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    SingleInstance(QString serverName, QObject *parent);
    ~SingleInstance() override;

    // Per-user, per-data-directory local socket name.
    static QString serverNameFor(const QString &dataDir);

    // Returns true if this process is now the primary instance. Returns false if another
    // instance is running and has received `arguments` — the caller should exit.
    bool start(const QStringList &arguments);

signals:
    void activationRequested(const QStringList &arguments);

private:
    void onNewConnection();

    QString m_serverName;
    QLocalServer *m_server = nullptr;
};

} // namespace owl::app
