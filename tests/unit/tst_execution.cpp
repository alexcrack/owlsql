// SPDX-License-Identifier: GPL-3.0-or-later
// ConnectionWorker + QueryJob against the scriptable fake driver (M1.4, specs/driver-api.md §6).
#include "fakedriver.h"
#include "owl/core/connectionworker.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using owl::ConnectionWorker;
using owl::QueryJob;

namespace {

owl::ConnectionParams params(const QString &host = u"localhost"_s)
{
    owl::ConnectionParams p;
    p.driverId = u"fake"_s;
    p.host = host;
    return p;
}

owl::Statement sql(const QString &text)
{
    owl::Statement s;
    s.sql = text;
    return s;
}

// Collects everything a job delivers.
struct Recorder
{
    explicit Recorder(QueryJob *job)
    {
        QObject::connect(job, &QueryJob::rowsReady, job, [this](const owl::RowBatch &b) {
            batches.append(b);
            rows += b.rowCount();
        });
        QObject::connect(job, &QueryJob::columnsReady, job,
                         [this](const QList<owl::ColumnInfo> &c, int index) {
                             columns.append(c);
                             columnIndexes.append(index);
                         });
        QObject::connect(job, &QueryJob::resultFinished, job,
                         [this](int index, qint64 affected) { results.append({index, affected}); });
        QObject::connect(job, &QueryJob::message, job,
                         [this](const owl::ServerMessage &m) { messages.append(m); });
    }
    QList<owl::RowBatch> batches;
    qint64 rows = 0;
    QList<QList<owl::ColumnInfo>> columns;
    QList<int> columnIndexes;
    QList<std::pair<int, qint64>> results;
    QList<owl::ServerMessage> messages;
};

bool waitForState(QueryJob *job, QueryJob::State state, int timeoutMs = 5000)
{
    return QTest::qWaitFor([&] { return job->state() == state; }, timeoutMs);
}

} // namespace

class TestExecution : public QObject
{
    Q_OBJECT

    owl::test::FakeDriver m_driver;

    std::unique_ptr<ConnectionWorker> connected()
    {
        auto worker = std::make_unique<ConnectionWorker>(&m_driver);
        QSignalSpy opened(worker.get(), &ConnectionWorker::opened);
        worker->open(params());
        if (!opened.wait(5000))
            return nullptr;
        return worker;
    }

private slots:
    void opensAndReportsServerInfo()
    {
        auto worker = connected();
        QVERIFY(worker);
        QCOMPARE(worker->state(), ConnectionWorker::State::Connected);
        QCOMPARE(worker->serverInfo().product, u"Fake"_s);
    }

    void openFailuresAreReported_data()
    {
        QTest::addColumn<QString>("host");
        QTest::addColumn<owl::DbError::Category>("category");
        QTest::newRow("connection") << u"fail"_s << owl::DbError::Category::Connection;
        QTest::newRow("auth") << u"auth"_s << owl::DbError::Category::Auth;
    }

    void openFailuresAreReported()
    {
        QFETCH(QString, host);
        QFETCH(owl::DbError::Category, category);
        ConnectionWorker worker(&m_driver);
        QSignalSpy failed(&worker, &ConnectionWorker::openFailed);
        worker.open(params(host));
        QVERIFY(failed.wait(5000));
        QCOMPARE(worker.lastError().category, category);
        QCOMPARE(worker.state(), ConnectionWorker::State::Disconnected);

        // Statements on a connection that failed to open fail cleanly.
        QueryJob *job = worker.execute(sql(u"rows 1"_s));
        QVERIFY(waitForState(job, QueryJob::State::Failed));
        QCOMPARE(job->error().category, owl::DbError::Category::Connection);
    }

    void smallResultFinishesComplete()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *job = worker->execute(sql(u"rows 10"_s));
        Recorder r(job);
        QVERIFY(waitForState(job, QueryJob::State::Finished));
        QVERIFY(job->isComplete());
        QCOMPARE(r.rows, 10);
        QCOMPARE(job->rowsFetched(), 10);
        QCOMPARE(r.columns.size(), 1);
        QCOMPARE(r.columns.first().first().name, u"id"_s);
        QCOMPARE(r.batches.last().at(r.batches.last().rowCount() - 1, 0).toInt(), 9);
        QCOMPARE(r.results.size(), 1);
    }

    // §6.2: pause after fetchChunk rows with the cursor open; fetchMore / fetchAll continue.
    void pausesAtFetchChunkAndContinues()
    {
        auto worker = connected();
        QVERIFY(worker);
        owl::ExecOptions options;
        options.fetchChunk = 1000;
        QueryJob *job = worker->execute(sql(u"rows 2500"_s), options);
        Recorder r(job);

        QVERIFY(waitForState(job, QueryJob::State::Paused));
        QCOMPARE(r.rows, 1000);
        job->fetchMore(1000);
        QTRY_COMPARE(r.rows, 2000);
        QVERIFY(waitForState(job, QueryJob::State::Paused));
        job->fetchAll();
        QVERIFY(waitForState(job, QueryJob::State::Finished));
        QCOMPARE(r.rows, 2500);
        QVERIFY(job->isComplete());

        // Batches are contiguous and numbered from the start of the result.
        qint64 expected = 0;
        for (const owl::RowBatch &b : std::as_const(r.batches)) {
            QCOMPARE(b.firstRow, expected);
            QCOMPARE(b.at(0, 0).toInt(), expected);
            expected += b.rowCount();
        }
    }

    void errorsCarryPosition()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *job = worker->execute(sql(u"error 17"_s));
        QSignalSpy failed(job, &QueryJob::failed);
        QVERIFY(waitForState(job, QueryJob::State::Failed));
        QCOMPARE(failed.count(), 1);
        QCOMPARE(job->error().category, owl::DbError::Category::Syntax);
        QCOMPARE(job->error().position, 17);
        QCOMPARE(job->error().sqlState, u"42601"_s);
    }

    void cancelInterruptsARunningStatement()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *job = worker->execute(sql(u"sleep 10000"_s));
        QVERIFY(waitForState(job, QueryJob::State::Running));
        QElapsedTimer t;
        t.start();
        job->cancel();
        QVERIFY(waitForState(job, QueryJob::State::Cancelled, 2000));
        QVERIFY2(t.elapsed() < 1000, qPrintable(QString::number(t.elapsed())));
        QCOMPARE(job->error().category, owl::DbError::Category::Cancelled);

        // The connection is still usable afterwards.
        QueryJob *next = worker->execute(sql(u"rows 3"_s));
        QVERIFY(waitForState(next, QueryJob::State::Finished));
        QCOMPARE(next->rowsFetched(), 3);
    }

    void cancelledQueuedJobNeverRuns()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *blocker = worker->execute(sql(u"sleep 300"_s));
        QueryJob *queued = worker->execute(sql(u"rows 5"_s));
        Recorder r(queued);
        QCOMPARE(queued->state(), QueryJob::State::Queued);
        queued->cancel();
        QCOMPARE(queued->state(), QueryJob::State::Cancelled); // reported immediately
        QVERIFY(waitForState(blocker, QueryJob::State::Finished));
        QTest::qWait(50);
        QCOMPARE(r.rows, 0);
    }

    void cancelPausedJob()
    {
        auto worker = connected();
        QVERIFY(worker);
        owl::ExecOptions options;
        options.fetchChunk = 100;
        QueryJob *job = worker->execute(sql(u"rows 1000"_s), options);
        QVERIFY(waitForState(job, QueryJob::State::Paused));
        job->cancel();
        QVERIFY(waitForState(job, QueryJob::State::Cancelled));
    }

    // §6.2: a new statement on the same worker closes a paused job first.
    void nextStatementClosesPausedJob()
    {
        auto worker = connected();
        QVERIFY(worker);
        owl::ExecOptions options;
        options.fetchChunk = 100;
        QueryJob *first = worker->execute(sql(u"rows 1000"_s), options);
        QVERIFY(waitForState(first, QueryJob::State::Paused));
        QueryJob *second = worker->execute(sql(u"rows 2"_s));
        QVERIFY(waitForState(second, QueryJob::State::Finished));
        QCOMPARE(first->state(), QueryJob::State::Finished);
        QVERIFY(!first->isComplete());
        QCOMPARE(first->rowsFetched(), 100);
    }

    void jobsRunInOrder()
    {
        auto worker = connected();
        QVERIFY(worker);
        QList<int> order;
        QList<QueryJob *> jobs;
        for (int i = 0; i < 5; ++i) {
            QueryJob *job = worker->execute(sql(u"rows %1"_s.arg(i + 1)));
            connect(job, &QueryJob::finished, this, [&order, i] { order.append(i); });
            jobs.append(job);
        }
        QVERIFY(waitForState(jobs.last(), QueryJob::State::Finished));
        QCOMPARE(order, (QList<int>{0, 1, 2, 3, 4}));
    }

    void messagesAndAffectedRows()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *notice = worker->execute(sql(u"notice table \"t\" does not exist, skipping"_s));
        Recorder rn(notice);
        QueryJob *update = worker->execute(sql(u"update 7"_s));
        Recorder ru(update);
        QVERIFY(waitForState(update, QueryJob::State::Finished));
        QCOMPARE(rn.messages.size(), 1);
        QCOMPARE(rn.messages.first().text, u"table \"t\" does not exist, skipping"_s);
        QCOMPARE(ru.results.size(), 1);
        QCOMPARE(ru.results.first().second, 7);
        QVERIFY(ru.columns.isEmpty()); // no row set
    }

    void multipleResultSets()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *job = worker->execute(sql(u"multi"_s));
        Recorder r(job);
        QVERIFY(waitForState(job, QueryJob::State::Finished));
        QCOMPARE(r.columnIndexes, (QList<int>{0, 1}));
        QCOMPARE(r.columns.at(1).first().name, u"key"_s);
        QCOMPARE(r.results.size(), 2);
        QCOMPARE(r.rows, 5);
        QCOMPARE(r.batches.last().firstRow, 0); // row numbers restart per result
    }

    void deletingAJobMidRunIsSafe()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *job = worker->execute(sql(u"sleep 5000"_s));
        QVERIFY(waitForState(job, QueryJob::State::Running));
        delete job; // cancels; its updates are dropped
        QueryJob *next = worker->execute(sql(u"rows 1"_s));
        QVERIFY(waitForState(next, QueryJob::State::Finished, 2000));
    }

    void destroyingTheWorkerStopsQuickly()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *job = worker->execute(sql(u"sleep 10000"_s));
        QVERIFY(waitForState(job, QueryJob::State::Running));
        worker->execute(sql(u"rows 5"_s));
        QElapsedTimer t;
        t.start();
        worker.reset(); // cancels the running statement, joins the thread, deletes the jobs
        QVERIFY2(t.elapsed() < 2000, qPrintable(QString::number(t.elapsed())));
    }

    void closeCancelsQueuedWork()
    {
        auto worker = connected();
        QVERIFY(worker);
        QueryJob *job = worker->execute(sql(u"sleep 200"_s));
        QueryJob *queued = worker->execute(sql(u"rows 5"_s));
        QSignalSpy closed(worker.get(), &ConnectionWorker::closed);
        QVERIFY(waitForState(job, QueryJob::State::Finished));
        worker->close();
        QVERIFY(closed.wait(2000) || closed.count() == 1);
        QCOMPARE(worker->state(), ConnectionWorker::State::Closed);
        Q_UNUSED(queued)
    }
};

QTEST_GUILESS_MAIN(TestExecution)
#include "tst_execution.moc"
