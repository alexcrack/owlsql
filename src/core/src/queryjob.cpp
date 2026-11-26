// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/queryjob.h"

#include "owl/core/connectionworker.h"

namespace owl {

QueryJob::QueryJob(ConnectionWorker *worker, std::shared_ptr<detail::JobShared> shared,
                   QObject *parent)
    : QObject(parent)
    , m_worker(worker)
    , m_shared(std::move(shared))
{ }

QueryJob::~QueryJob()
{
    if (!m_worker)
        return; // the worker is being destroyed and has already stopped everything
    if (!isDone()) {
        m_shared->cancelRequested = true;
        m_worker->requestCancel(m_shared->id);
    }
    m_worker->jobDestroyed(m_shared->id);
}

bool QueryJob::isDone() const
{
    return m_state == State::Finished || m_state == State::Failed || m_state == State::Cancelled;
}

qint64 QueryJob::elapsedMs() const
{
    if (m_finalElapsed >= 0)
        return m_finalElapsed;
    return m_timer.isValid() ? m_timer.elapsed() : 0;
}

void QueryJob::fetchMore(qsizetype rows)
{
    if (m_state != State::Paused || !m_worker || rows <= 0)
        return;
    setState(State::Running);
    m_worker->requestFetch(m_shared->id, rows);
}

void QueryJob::fetchAll()
{
    if (m_state != State::Paused || !m_worker)
        return;
    setState(State::Running);
    m_worker->requestFetch(m_shared->id, -1);
}

void QueryJob::cancel()
{
    if (isDone())
        return;
    m_shared->cancelRequested = true;
    if (m_worker)
        m_worker->requestCancel(m_shared->id);
    else
        onCancelled();
}

void QueryJob::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    if (isDone() && m_timer.isValid())
        m_finalElapsed = m_timer.elapsed();
    emit stateChanged();
}

void QueryJob::onStarted()
{
    if (isDone())
        return;
    if (!m_timer.isValid())
        m_timer.start();
    setState(State::Running);
}

void QueryJob::onColumns(const QList<ColumnInfo> &columns, int resultIndex)
{
    if (!isDone())
        emit columnsReady(columns, resultIndex);
}

void QueryJob::onRows(const RowBatch &batch)
{
    if (isDone())
        return;
    m_rowsFetched += batch.rowCount();
    emit rowsReady(batch);
    emit progress();
}

void QueryJob::onMessage(const ServerMessage &serverMessage)
{
    if (!isDone())
        emit message(serverMessage);
}

void QueryJob::onResultFinished(int resultIndex, qint64 affectedRows)
{
    if (!isDone())
        emit resultFinished(resultIndex, affectedRows);
}

void QueryJob::onPaused()
{
    if (!isDone())
        setState(State::Paused);
}

void QueryJob::onFinished(bool complete)
{
    if (isDone())
        return;
    m_complete = complete;
    setState(State::Finished);
    emit finished();
}

void QueryJob::onFailed(const DbError &error)
{
    if (isDone())
        return;
    m_error = error;
    setState(State::Failed);
    emit failed(error);
}

void QueryJob::onCancelled()
{
    if (isDone())
        return;
    DbError error;
    error.category = DbError::Category::Cancelled;
    error.message = QStringLiteral("Cancelled");
    m_error = error;
    setState(State::Cancelled);
    emit failed(error);
}

} // namespace owl
