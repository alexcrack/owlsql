// SPDX-License-Identifier: GPL-3.0-or-later
#include "conformance.h"

#include "owl/core/connectionworker.h"
#include "owl/core/driverregistry.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>
#include <QThread>

using namespace Qt::StringLiterals;

namespace owl::conformance {

namespace {

// Runs a statement synchronously and collects every row of the first result.
struct Rows
{
    QList<ColumnInfo> columns;
    QList<QList<Value>> rows;
    qint64 affected = -1;
};

Result<Rows> runAll(IConnection &conn, const QString &sql, QList<Value> params = {},
                    ExecOptions options = {})
{
    Statement statement;
    statement.sql = sql;
    statement.params = std::move(params);
    Result<std::unique_ptr<IResultCursor>> cursor = conn.execute(statement, options);
    if (!cursor)
        return fail(cursor.error());
    Rows out;
    out.columns = (*cursor)->columns();
    while (!out.columns.isEmpty() && !(*cursor)->atEnd()) {
        Result<RowBatch> batch = (*cursor)->fetch(500);
        if (!batch)
            return fail(batch.error());
        for (qsizetype r = 0; r < batch->rowCount(); ++r) {
            QList<Value> row;
            for (qsizetype c = 0; c < batch->columnCount; ++c)
                row.append(batch->at(r, c));
            out.rows.append(row);
        }
        if (batch->cells.isEmpty())
            break;
    }
    out.affected = (*cursor)->affectedRows();
    (*cursor)->close();
    return out;
}

} // namespace

class DriverConformance : public QObject
{
    Q_OBJECT

    Fixtures m_f;
    DriverRegistry m_registry;
    IDriver *m_driver = nullptr;
    std::unique_ptr<IConnection> m_conn;

    std::unique_ptr<IConnection> openConnection()
    {
        std::unique_ptr<IConnection> conn = m_driver->createConnection();
        if (Result<void> opened = conn->open(m_f.params); !opened)
            qWarning("open failed: %s", qPrintable(opened.error().toString()));
        return conn;
    }

    std::unique_ptr<ConnectionWorker> openWorker()
    {
        auto worker = std::make_unique<ConnectionWorker>(m_driver);
        QSignalSpy opened(worker.get(), &ConnectionWorker::opened);
        worker->open(m_f.params);
        if (!opened.wait(10000))
            return nullptr;
        return worker;
    }

public:
    explicit DriverConformance(Fixtures fixtures)
        : m_f(std::move(fixtures))
    { }

private slots:
    void initTestCase()
    {
        if (!m_f.skipReason.isEmpty())
            QSKIP(qPrintable(m_f.skipReason));
        const QStringList problems = m_registry.loadPlugins(m_f.pluginDirectory);
        for (const QString &p : problems)
            qWarning("plugin: %s", qPrintable(p));
        m_driver = m_registry.driver(m_f.driverId);
        QVERIFY2(m_driver,
                 qPrintable(u"driver plugin '%1' not found in %2"_s.arg(m_f.driverId,
                                                                        m_f.pluginDirectory)));
        m_conn = openConnection();
        QVERIFY(m_conn->isOpen());
    }

    void cleanupTestCase()
    {
        if (m_conn)
            m_conn->close();
    }

    void identifiesServer()
    {
        const ServerInfo info = m_conn->serverInfo();
        qInfo("%s %s", qPrintable(info.product), qPrintable(info.version));
        QVERIFY(!info.product.isEmpty());
        QVERIFY(info.versionNumber > 0);
        QVERIFY(m_conn->ping());
    }

    void scalarTypes()
    {
        const Result<Rows> r = runAll(*m_conn, m_f.selectScalars);
        QVERIFY2(r, qPrintable(r ? QString() : r.error().toString()));
        QCOMPARE(r->columns.size(), 8);
        QCOMPARE(r->rows.size(), 1);
        const QList<Value> &row = r->rows.first();
        QCOMPARE(row.at(0).toInt(), 42);
        QCOMPARE(row.at(1).toInt(), Q_INT64_C(4611686018427387904));
        QCOMPARE(row.at(2).toInt(), -7);
        QCOMPARE(row.at(3).kind(), ValueKind::Double);
        QCOMPARE(row.at(3).toDouble(), 2.5);
        QCOMPARE(row.at(4).toText(), u"Ωmega 🦉"_s);
        QVERIFY(row.at(5).isNull());
        QCOMPARE(row.at(6).kind(), ValueKind::Text); // empty text is not NULL
        QCOMPARE(row.at(6).toText(), QString(u""_s));
        QCOMPARE(row.at(7).kind(), ValueKind::Bytes);
        QCOMPARE(row.at(7).toBytes(), QByteArray("\x00\xff\x10", 3));
    }

    void boundParameters()
    {
        const Result<Rows> r = runAll(*m_conn, m_f.selectThreeParams,
                                      {Value::fromInt(7), Value::text(u"it's"_s), Value()});
        QVERIFY2(r, qPrintable(r ? QString() : r.error().toString()));
        QCOMPARE(r->rows.size(), 1);
        QCOMPARE(r->rows.first().at(0).toInt(), 7);
        QCOMPARE(r->rows.first().at(1).toText(), u"it's"_s);
        QVERIFY(r->rows.first().at(2).isNull());

        const Result<Rows> wrongCount = runAll(*m_conn, m_f.selectThreeParams, {Value::fromInt(1)});
        QVERIFY(!wrongCount);
    }

    void syntaxErrorHasCategoryAndPosition()
    {
        const Result<Rows> r = runAll(*m_conn, m_f.syntaxError);
        QVERIFY(!r);
        qInfo("%s (position %d)", qPrintable(r.error().toString()), r.error().position);
        QCOMPARE(r.error().category, DbError::Category::Syntax);
        if (m_f.syntaxErrorPosition >= 0)
            QCOMPARE(r.error().position, m_f.syntaxErrorPosition);
    }

    void constraintsAndAffectedRows()
    {
        QVERIFY(runAll(*m_conn, m_f.createUniqueTable));
        const Result<Rows> inserted = runAll(*m_conn, m_f.insertThreeRows);
        QVERIFY(inserted);
        QCOMPARE(inserted->affected, 3);
        const Result<Rows> duplicate = runAll(*m_conn, m_f.insertDuplicate);
        QVERIFY(!duplicate);
        QCOMPARE(duplicate.error().category, DbError::Category::Constraint);
        const Result<Rows> updated = runAll(*m_conn, m_f.updateAllRows);
        QVERIFY(updated);
        QCOMPARE(updated->affected, 3);
    }

    void transactionState()
    {
        QCOMPARE(m_conn->txState(), TxState::Idle);
        QVERIFY(runAll(*m_conn, u"BEGIN"_s));
        QCOMPARE(m_conn->txState(), TxState::InTransaction);
        QVERIFY(runAll(*m_conn, u"ROLLBACK"_s));
        QCOMPARE(m_conn->txState(), TxState::Idle);
    }

    void largeValuesAreTruncated()
    {
        ExecOptions options;
        options.previewLimitBytes = 64 * 1024;
        const Result<Rows> r = runAll(*m_conn, m_f.bigText(200'000), {}, options);
        QVERIFY2(r, qPrintable(r ? QString() : r.error().toString()));
        const Value &v = r->rows.first().first();
        QVERIFY(v.isTruncated());
        QCOMPARE(v.fullSize(), 200'000);
        QVERIFY(v.toText().size() <= qsizetype{64} * 1024);
        QVERIFY(v.toText().startsWith(u"xxxx"_s));
    }

    void readOnlyGuard()
    {
        ExecOptions options;
        options.readOnly = true;
        const Result<Rows> write = runAll(*m_conn, m_f.writeStatement, {}, options);
        QVERIFY(!write);
        QCOMPARE(write.error().category, DbError::Category::Permission);
        QVERIFY(runAll(*m_conn, m_f.generateRows(3), {}, options)); // reads still work
    }

    void emptyStatement()
    {
        const Result<Rows> r = runAll(*m_conn, u"-- nothing to do"_s);
        QVERIFY2(r, qPrintable(r ? QString() : r.error().toString()));
        QVERIFY(r->columns.isEmpty());
    }

    // Through the worker: streaming with pause / fetchAll, as the UI uses it.
    void streamsTenThousandRows()
    {
        std::unique_ptr<ConnectionWorker> worker = openWorker();
        QVERIFY(worker);
        ExecOptions options;
        options.fetchChunk = 1000;
        Statement statement;
        statement.sql = m_f.generateRows(10'000);
        QueryJob *job = worker->execute(statement, options);
        qint64 expected = 1;
        bool inOrder = true;
        connect(job, &QueryJob::rowsReady, job, [&](const RowBatch &b) {
            for (qsizetype r = 0; r < b.rowCount(); ++r)
                inOrder = inOrder && b.at(r, 0).toInt() == expected++;
        });
        QTRY_COMPARE(job->state(), QueryJob::State::Paused);
        QCOMPARE(job->rowsFetched(), 1000);
        job->fetchAll();
        QTRY_COMPARE_WITH_TIMEOUT(job->state(), QueryJob::State::Finished, 20000);
        QCOMPARE(job->rowsFetched(), 10'000);
        QVERIFY(inOrder);
    }

    void cancelsARunawayQuery()
    {
        std::unique_ptr<ConnectionWorker> worker = openWorker();
        QVERIFY(worker);
        Statement statement;
        statement.sql = m_f.runaway;
        QueryJob *job = worker->execute(statement);
        QTRY_COMPARE(job->state(), QueryJob::State::Running);
        QTest::qWait(200);
        QElapsedTimer t;
        t.start();
        job->cancel();
        QTRY_COMPARE_WITH_TIMEOUT(job->state(), QueryJob::State::Cancelled, 5000);
        qInfo("cancelled after %lld ms", static_cast<long long>(t.elapsed()));
        QVERIFY(t.elapsed() < 3000);

        statement.sql = m_f.generateRows(2);
        QueryJob *next = worker->execute(statement);
        QTRY_COMPARE(next->state(), QueryJob::State::Finished);
        QCOMPARE(next->rowsFetched(), 2);
    }
};

int runConformance(int argc, char **argv, const Fixtures &fixtures)
{
    QCoreApplication app(argc, argv);
    DriverConformance test(fixtures);
    return QTest::qExec(&test, argc, argv);
}

} // namespace owl::conformance

#include "conformance.moc"
