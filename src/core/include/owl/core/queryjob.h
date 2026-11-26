// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/driver.h"

#include <QElapsedTimer>
#include <QObject>

#include <atomic>
#include <memory>

namespace owl {

class ConnectionWorker;

namespace detail {
// State shared between a QueryJob (main thread) and the worker thread running it.
struct JobShared
{
    quint64 id = 0;
    Statement statement;
    ExecOptions options;
    std::atomic<bool> cancelRequested{false};
};
} // namespace detail

// One statement running on a ConnectionWorker (docs/specs/driver-api.md §6.2).
//
//   Queued → Running → (Paused ⇄ Running)* → Finished
//                    ↘ Failed / Cancelled
//
// Results stream in as rowsReady batches. After options.fetchChunk rows the job pauses with the
// cursor open; fetchMore()/fetchAll() continue. A new statement queued on the same worker closes
// a paused job (it then finishes with isComplete() == false). Lives on the main thread.
class QueryJob : public QObject
{
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged FINAL)
    Q_PROPERTY(qint64 rowsFetched READ rowsFetched NOTIFY progress FINAL)
    Q_PROPERTY(qint64 elapsedMs READ elapsedMs NOTIFY progress FINAL)

public:
    enum class State : quint8 { Queued, Running, Paused, Finished, Failed, Cancelled };
    Q_ENUM(State)

    ~QueryJob() override;

    State state() const { return m_state; }
    bool isDone() const;
    // All rows of all results were read (false when a paused job was closed early).
    bool isComplete() const { return m_complete; }
    qint64 rowsFetched() const { return m_rowsFetched; }
    qint64 elapsedMs() const;
    const Statement &statement() const { return m_shared->statement; }
    DbError error() const { return m_error; }
    quint64 id() const { return m_shared->id; }

    Q_INVOKABLE void fetchMore(qsizetype rows = 1000);
    Q_INVOKABLE void fetchAll();
    Q_INVOKABLE void cancel();

signals:
    void stateChanged();
    void columnsReady(const QList<owl::ColumnInfo> &columns, int resultIndex);
    void rowsReady(const owl::RowBatch &batch);
    void message(const owl::ServerMessage &message);
    void resultFinished(int resultIndex, qint64 affectedRows);
    void finished();
    void failed(const owl::DbError &error);
    void progress();

private:
    friend class ConnectionWorker;
    QueryJob(ConnectionWorker *worker, std::shared_ptr<detail::JobShared> shared, QObject *parent);

    // Called by ConnectionWorker on the main thread.
    void onStarted();
    void onColumns(const QList<ColumnInfo> &columns, int resultIndex);
    void onRows(const RowBatch &batch);
    void onMessage(const ServerMessage &serverMessage);
    void onResultFinished(int resultIndex, qint64 affectedRows);
    void onPaused();
    void onFinished(bool complete);
    void onFailed(const DbError &error);
    void onCancelled();
    void setState(State state);

    ConnectionWorker *m_worker;
    std::shared_ptr<detail::JobShared> m_shared;
    State m_state = State::Queued;
    bool m_complete = false;
    qint64 m_rowsFetched = 0;
    DbError m_error;
    QElapsedTimer m_timer;
    qint64 m_finalElapsed = -1;
};

} // namespace owl
