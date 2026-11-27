// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/driver.h"
#include "owl/core/queryjob.h"

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPointer>

#include <atomic>
#include <memory>

class QThread;

namespace owl {

// One physical connection on its own thread with a FIFO job queue (docs/specs/driver-api.md §6.1,
// decision 042). Lives on the main thread; the driver's blocking calls run on the worker thread.
// The IDriver must outlive the worker.
class ConnectionWorker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged FINAL)

public:
    enum class State : quint8 { Disconnected, Connecting, Connected, Closed };
    Q_ENUM(State)

    explicit ConnectionWorker(IDriver *driver, QObject *parent = nullptr);
    // Cancels running work, stops the thread and closes the connection.
    ~ConnectionWorker() override;

    // Connects, then runs `startupStatements` (a session's startup script, already split; logged
    // as Internal). A failing startup statement fails the open.
    void open(const ConnectionParams &params, const QStringList &startupStatements = {});
    void close();

    // Queues a statement. The job is a child of this worker unless reparented; jobs queued
    // before open() completes run after it (and fail if the connection could not be opened).
    QueryJob *execute(const Statement &statement, const ExecOptions &options = {});

    // Runs `task` on the worker thread, in queue order with statements (a paused job is closed
    // first, as for a statement). The connection is null when it is not open. The task must
    // close the cursors it opens and posts its results back itself (catalog calls, CatalogCache).
    void runTask(std::function<void(IConnection *)> task);

    State state() const { return m_state; }
    ServerInfo serverInfo() const { return m_serverInfo; }
    DbError lastError() const { return m_lastError; }

signals:
    void stateChanged();
    void opened();
    void openFailed(const owl::DbError &error);
    void closed();

private:
    friend class QueryJob;
    class Executor;

    // From QueryJob (main thread).
    void requestFetch(quint64 jobId, qint64 rows);
    void requestCancel(quint64 jobId);
    void jobDestroyed(quint64 jobId);

    // From the Executor, delivered on the main thread.
    void onOpened(const ServerInfo &info);
    void onOpenFailed(const DbError &error);
    void onClosed();
    void jobStarted(quint64 id);
    void jobColumns(quint64 id, const QList<ColumnInfo> &columns, int resultIndex);
    void jobRows(quint64 id, const RowBatch &batch);
    void jobMessage(quint64 id, const ServerMessage &message);
    void jobResultFinished(quint64 id, int resultIndex, qint64 affectedRows);
    void jobPaused(quint64 id);
    void jobFinished(quint64 id, bool complete);
    void jobFailed(quint64 id, const DbError &error);
    void jobCancelled(quint64 id);

    // Runs fn on the worker thread.
    template<typename Fn>
    void post(Fn &&fn);
    QueryJob *job(quint64 id) const { return m_jobs.value(id); }
    void setState(State state);

    QThread *m_thread;
    Executor *m_executor; // lives on m_thread
    QHash<quint64, QPointer<QueryJob>> m_jobs;
    quint64 m_nextJobId = 1;
    State m_state = State::Disconnected;
    ServerInfo m_serverInfo;
    DbError m_lastError;

    // Shared with the worker thread.
    QMutex m_cancelMutex;
    std::shared_ptr<ICancelHandle> m_cancelHandle;
    std::atomic<quint64> m_runningJob{0};
};

} // namespace owl
