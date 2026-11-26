// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/connectionworker.h"

#include <QMutexLocker>
#include <QThread>

#include <deque>

namespace owl {

namespace {
// Rows per rowsReady batch: small enough for a responsive first paint, large enough to keep
// queued-signal overhead low.
constexpr qsizetype batchRows = 256;
} // namespace

// Runs on the worker thread. Talks back to the main thread only through queued calls on
// `owner` (which lives there), so a deleted QueryJob or worker simply drops the updates.
class ConnectionWorker::Executor : public QObject
{
public:
    Executor(ConnectionWorker *owner, IDriver *driver)
        : m_owner(owner)
        , m_driver(driver)
    { }

    void open(const ConnectionParams &params)
    {
        closeAll(/*reportClosed=*/false);
        m_connection = m_driver->createConnection();
        Result<void> result = m_connection->open(params);
        if (!result) {
            m_connection.reset();
            toMain([o = m_owner, e = result.error()] { o->onOpenFailed(e); });
            return;
        }
        {
            QMutexLocker lock(&m_owner->m_cancelMutex);
            m_owner->m_cancelHandle = m_connection->cancelHandle();
        }
        toMain([o = m_owner, info = m_connection->serverInfo()] { o->onOpened(info); });
    }

    void close() { closeAll(/*reportClosed=*/true); }

    void enqueue(std::shared_ptr<detail::JobShared> job)
    {
        m_queue.push_back(std::move(job));
        if (!m_current || m_paused)
            runQueue();
    }

    // rows < 0: everything that is left.
    void fetch(quint64 jobId, qint64 rows)
    {
        if (!m_current || m_current->id != jobId || !m_paused)
            return;
        m_paused = false;
        m_owner->m_runningJob = jobId;
        toMain([o = m_owner, jobId] { o->jobStarted(jobId); });
        stream(rows);
        runQueue();
    }

    void cancelPaused(quint64 jobId)
    {
        if (!m_current || m_current->id != jobId || !m_paused)
            return;
        endCurrent(Outcome::Cancelled);
        runQueue();
    }

private:
    enum class Outcome : quint8 { Complete, Incomplete, Failed, Cancelled };

    template<typename Fn>
    void toMain(Fn &&fn)
    {
        QMetaObject::invokeMethod(m_owner, std::forward<Fn>(fn), Qt::QueuedConnection);
    }

    // Starts queued jobs until one pauses or the queue is empty. A paused job is closed first
    // when something else is waiting (specs/driver-api.md §6.2).
    void runQueue()
    {
        while (!m_queue.empty()) {
            if (m_current) {
                if (!m_paused)
                    return;
                endCurrent(Outcome::Incomplete);
            }
            std::shared_ptr<detail::JobShared> job = std::move(m_queue.front());
            m_queue.pop_front();
            if (job->cancelRequested)
                continue; // already reported as cancelled by the main thread
            run(std::move(job));
        }
    }

    void run(std::shared_ptr<detail::JobShared> job)
    {
        m_current = std::move(job);
        m_resultIndex = 0;
        m_rowsInResult = 0;
        m_paused = false;
        const quint64 id = m_current->id;
        m_owner->m_runningJob = id;
        toMain([o = m_owner, id] { o->jobStarted(id); });

        if (!m_connection || !m_connection->isOpen()) {
            DbError error;
            error.category = DbError::Category::Connection;
            error.message = QStringLiteral("Not connected");
            endCurrent(Outcome::Failed, error);
            return;
        }

        Result<std::unique_ptr<IResultCursor>> cursor
            = m_connection->execute(m_current->statement, m_current->options);
        flushMessages();
        if (!cursor) {
            endCurrent(Outcome::Failed, cursor.error());
            return;
        }
        m_cursor = std::move(*cursor);
        announceColumns();
        stream(m_current->options.fetchChunk);
    }

    // Delivers up to `limit` rows (all if negative), across result sets; pauses at the limit.
    void stream(qint64 limit)
    {
        qint64 delivered = 0;
        for (;;) {
            if (m_current->cancelRequested) {
                endCurrent(Outcome::Cancelled);
                return;
            }
            const bool hasRows = !m_cursor->columns().isEmpty();
            if (!hasRows || m_cursor->atEnd()) {
                if (!finishResult())
                    return; // job finished
                continue; // next result set
            }
            if (limit >= 0 && delivered >= limit) {
                pause();
                return;
            }
            const qsizetype want
                = limit < 0 ? batchRows : qMin<qint64>(batchRows, limit - delivered);
            Result<RowBatch> batch = m_cursor->fetch(want);
            flushMessages();
            if (!batch) {
                endCurrent(Outcome::Failed, batch.error());
                return;
            }
            if (batch->cells.isEmpty()) {
                if (!finishResult())
                    return;
                continue;
            }
            batch->firstRow = m_rowsInResult;
            const qsizetype rows = batch->rowCount();
            m_rowsInResult += rows;
            delivered += rows;
            toMain([o = m_owner, id = m_current->id, b = std::move(*batch)] { o->jobRows(id, b); });
        }
    }

    // Reports the end of the current result; moves to the next one if any.
    // Returns false when the whole job is finished.
    bool finishResult()
    {
        toMain(
            [o = m_owner, id = m_current->id, index = m_resultIndex,
             affected = m_cursor->affectedRows()] { o->jobResultFinished(id, index, affected); });
        if (m_cursor->nextResult()) {
            ++m_resultIndex;
            m_rowsInResult = 0;
            announceColumns();
            return true;
        }
        endCurrent(Outcome::Complete);
        return false;
    }

    void pause()
    {
        m_paused = true;
        m_owner->m_runningJob = 0;
        toMain([o = m_owner, id = m_current->id] { o->jobPaused(id); });
    }

    void announceColumns()
    {
        const QList<ColumnInfo> columns = m_cursor->columns();
        if (columns.isEmpty())
            return;
        toMain([o = m_owner, id = m_current->id, columns, index = m_resultIndex] {
            o->jobColumns(id, columns, index);
        });
    }

    void flushMessages()
    {
        if (!m_connection || !m_current)
            return;
        const QList<ServerMessage> messages = m_connection->takeMessages();
        for (const ServerMessage &m : messages) {
            toMain([o = m_owner, id = m_current->id, m] { o->jobMessage(id, m); });
        }
    }

    void endCurrent(Outcome outcome, const DbError &error = {})
    {
        if (m_cursor) {
            m_cursor->close();
            m_cursor.reset();
        }
        const quint64 id = m_current->id;
        const bool cancelled = outcome == Outcome::Cancelled
            || (outcome == Outcome::Failed
                && (m_current->cancelRequested || error.category == DbError::Category::Cancelled));
        m_current.reset();
        m_paused = false;
        m_owner->m_runningJob = 0;

        if (cancelled) {
            toMain([o = m_owner, id] { o->jobCancelled(id); });
        } else if (outcome == Outcome::Failed) {
            toMain([o = m_owner, id, error] { o->jobFailed(id, error); });
        } else {
            toMain([o = m_owner, id, complete = outcome == Outcome::Complete] {
                o->jobFinished(id, complete);
            });
        }
    }

    void closeAll(bool reportClosed)
    {
        if (m_current)
            endCurrent(Outcome::Cancelled);
        for (const auto &job : m_queue) {
            toMain([o = m_owner, id = job->id] { o->jobCancelled(id); });
        }
        m_queue.clear();
        {
            QMutexLocker lock(&m_owner->m_cancelMutex);
            m_owner->m_cancelHandle.reset();
        }
        if (m_connection) {
            m_connection->close();
            m_connection.reset();
        }
        if (reportClosed)
            toMain([o = m_owner] { o->onClosed(); });
    }

public:
    // Final cleanup on the worker thread before it stops (no updates are delivered any more).
    void shutdown()
    {
        if (m_cursor) {
            m_cursor->close();
            m_cursor.reset();
        }
        m_current.reset();
        m_queue.clear();
        {
            QMutexLocker lock(&m_owner->m_cancelMutex);
            m_owner->m_cancelHandle.reset();
        }
        if (m_connection) {
            m_connection->close();
            m_connection.reset();
        }
    }

private:
    ConnectionWorker *m_owner;
    IDriver *m_driver;
    std::unique_ptr<IConnection> m_connection;
    std::deque<std::shared_ptr<detail::JobShared>> m_queue;
    std::shared_ptr<detail::JobShared> m_current;
    std::unique_ptr<IResultCursor> m_cursor;
    int m_resultIndex = 0;
    qint64 m_rowsInResult = 0;
    bool m_paused = false;
};

// --- ConnectionWorker (main thread) ------------------------------------------------------------

ConnectionWorker::ConnectionWorker(IDriver *driver, QObject *parent)
    : QObject(parent)
    , m_thread(new QThread)
    , m_executor(new Executor(this, driver))
{
    m_thread->setObjectName(QStringLiteral("owlsql-connection"));
    m_executor->moveToThread(m_thread);
    m_thread->start();
}

ConnectionWorker::~ConnectionWorker()
{
    // Jobs are our children and are deleted after this body: detach them first.
    for (const QPointer<QueryJob> &job : std::as_const(m_jobs)) {
        if (job) {
            job->m_shared->cancelRequested = true;
            job->m_worker = nullptr;
        }
    }
    {
        QMutexLocker lock(&m_cancelMutex);
        if (m_cancelHandle && m_runningJob != 0)
            (void)m_cancelHandle->cancel();
    }
    QMetaObject::invokeMethod(
        m_executor, [ex = m_executor] { ex->shutdown(); }, Qt::QueuedConnection);
    m_thread->quit();
    m_thread->wait();
    delete m_executor;
    delete m_thread;
}

template<typename Fn>
void ConnectionWorker::post(Fn &&fn)
{
    QMetaObject::invokeMethod(m_executor, std::forward<Fn>(fn), Qt::QueuedConnection);
}

void ConnectionWorker::open(const ConnectionParams &params)
{
    setState(State::Connecting);
    post([ex = m_executor, params] { ex->open(params); });
}

void ConnectionWorker::close()
{
    post([ex = m_executor] { ex->close(); });
}

QueryJob *ConnectionWorker::execute(const Statement &statement, const ExecOptions &options)
{
    auto shared = std::make_shared<detail::JobShared>();
    shared->id = m_nextJobId++;
    shared->statement = statement;
    shared->options = options;
    auto *job = new QueryJob(this, shared, this);
    m_jobs.insert(shared->id, job);
    post([ex = m_executor, shared] { ex->enqueue(shared); });
    return job;
}

void ConnectionWorker::requestFetch(quint64 jobId, qint64 rows)
{
    post([ex = m_executor, jobId, rows] { ex->fetch(jobId, rows); });
}

void ConnectionWorker::requestCancel(quint64 jobId)
{
    QueryJob *j = job(jobId);
    const QueryJob::State state = j ? j->state() : QueryJob::State::Cancelled;

    // Running (as far as the worker thread knows): interrupt the blocking call right now.
    if (m_runningJob == jobId) {
        QMutexLocker lock(&m_cancelMutex);
        if (m_cancelHandle)
            (void)m_cancelHandle->cancel();
    }
    if (state == QueryJob::State::Queued && j)
        j->onCancelled(); // the worker skips it when it gets there
    else if (state == QueryJob::State::Paused)
        post([ex = m_executor, jobId] { ex->cancelPaused(jobId); });
}

void ConnectionWorker::jobDestroyed(quint64 jobId)
{
    m_jobs.remove(jobId);
}

void ConnectionWorker::jobStarted(quint64 id)
{
    if (QueryJob *j = job(id))
        j->onStarted();
}

void ConnectionWorker::jobColumns(quint64 id, const QList<ColumnInfo> &columns, int resultIndex)
{
    if (QueryJob *j = job(id))
        j->onColumns(columns, resultIndex);
}

void ConnectionWorker::jobRows(quint64 id, const RowBatch &batch)
{
    if (QueryJob *j = job(id))
        j->onRows(batch);
}

void ConnectionWorker::jobMessage(quint64 id, const ServerMessage &message)
{
    if (QueryJob *j = job(id))
        j->onMessage(message);
}

void ConnectionWorker::jobResultFinished(quint64 id, int resultIndex, qint64 affectedRows)
{
    if (QueryJob *j = job(id))
        j->onResultFinished(resultIndex, affectedRows);
}

void ConnectionWorker::jobPaused(quint64 id)
{
    if (QueryJob *j = job(id))
        j->onPaused();
}

void ConnectionWorker::jobFinished(quint64 id, bool complete)
{
    if (QueryJob *j = job(id))
        j->onFinished(complete);
}

void ConnectionWorker::jobFailed(quint64 id, const DbError &error)
{
    if (QueryJob *j = job(id))
        j->onFailed(error);
}

void ConnectionWorker::jobCancelled(quint64 id)
{
    if (QueryJob *j = job(id))
        j->onCancelled();
}

void ConnectionWorker::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void ConnectionWorker::onOpened(const ServerInfo &info)
{
    m_serverInfo = info;
    setState(State::Connected);
    emit opened();
}

void ConnectionWorker::onOpenFailed(const DbError &error)
{
    m_lastError = error;
    setState(State::Disconnected);
    emit openFailed(error);
}

void ConnectionWorker::onClosed()
{
    setState(State::Closed);
    emit closed();
}

} // namespace owl
