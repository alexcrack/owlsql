// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakedriver.h"

#include <QElapsedTimer>
#include <QThread>

#include <atomic>

namespace owl::test {

namespace {

struct FakeResult
{
    QList<ColumnInfo> columns;
    qint64 rows = 0;
    qint64 affected = -1;
    bool secondShape = false; // different columns/values for the 2nd result of "multi"
};

ColumnInfo column(const QString &name, ValueKind kind)
{
    ColumnInfo c;
    c.name = name;
    c.kind = kind;
    return c;
}

class FakeCancel : public ICancelHandle
{
public:
    explicit FakeCancel(std::shared_ptr<std::atomic<bool>> flag)
        : m_flag(std::move(flag))
    { }
    Result<void> cancel() override
    {
        *m_flag = true;
        return {};
    }

private:
    std::shared_ptr<std::atomic<bool>> m_flag;
};

class FakeCursor : public IResultCursor
{
public:
    explicit FakeCursor(QList<FakeResult> results)
        : m_results(std::move(results))
    { }
    QList<ColumnInfo> columns() const override { return current().columns; }
    Result<RowBatch> fetch(qsizetype maxRows) override
    {
        const FakeResult &r = current();
        RowBatch batch;
        batch.firstRow = m_position;
        batch.columnCount = r.columns.size();
        const qint64 n = qMin<qint64>(maxRows, r.rows - m_position);
        for (qint64 i = 0; i < n; ++i, ++m_position) {
            if (r.secondShape) {
                batch.cells << Value::text(QStringLiteral("k%1").arg(m_position))
                            << Value::fromDouble(static_cast<double>(m_position) / 2);
            } else {
                batch.cells << Value::fromInt(m_position)
                            << Value::text(QStringLiteral("name %1").arg(m_position));
            }
        }
        return batch;
    }
    bool atEnd() const override { return m_position >= current().rows; }
    qint64 affectedRows() const override { return current().affected; }
    bool nextResult() override
    {
        if (m_index + 1 >= m_results.size())
            return false;
        ++m_index;
        m_position = 0;
        return true;
    }
    void close() override { m_closed = true; }

private:
    const FakeResult &current() const { return m_results.at(m_index); }
    QList<FakeResult> m_results;
    qsizetype m_index = 0;
    qint64 m_position = 0;
    bool m_closed = false;
};

class FakeConnection : public IConnection
{
public:
    Result<void> open(const ConnectionParams &params) override
    {
        if (params.host == QLatin1String("fail"))
            return fail(DbError::Category::Connection, QStringLiteral("could not connect to fail"));
        if (params.host == QLatin1String("auth"))
            return fail(DbError::Category::Auth, QStringLiteral("password authentication failed"));
        m_open = true;
        return {};
    }
    void close() override { m_open = false; }
    bool isOpen() const override { return m_open; }
    bool ping() override { return m_open; }
    ServerInfo serverInfo() const override
    {
        return {.product = QStringLiteral("Fake"),
                .version = QStringLiteral("1.0"),
                .versionNumber = 10000};
    }
    std::shared_ptr<ICancelHandle> cancelHandle() override
    {
        return std::make_shared<FakeCancel>(m_cancel);
    }

    Result<std::unique_ptr<IResultCursor>> execute(const Statement &statement,
                                                   const ExecOptions &) override
    {
        *m_cancel = false;
        const QStringList words = statement.sql.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString command = words.value(0);
        const QString argument = statement.sql.section(QLatin1Char(' '), 1);
        const QList<ColumnInfo> idName{column(QStringLiteral("id"), ValueKind::Int),
                                       column(QStringLiteral("name"), ValueKind::Text)};

        if (command == QLatin1String("rows"))
            return cursor({{.columns = idName, .rows = argument.toLongLong()}});
        if (command == QLatin1String("update"))
            return cursor({{.columns = {}, .affected = argument.toLongLong()}});
        if (command == QLatin1String("notice")) {
            m_messages.append({.severity = ServerMessage::Severity::Notice,
                               .text = argument,
                               .sqlState = QStringLiteral("00000")});
            return cursor({{}});
        }
        if (command == QLatin1String("multi")) {
            return cursor({{.columns = idName, .rows = 3},
                           {.columns = {column(QStringLiteral("key"), ValueKind::Text),
                                        column(QStringLiteral("half"), ValueKind::Double)},
                            .rows = 2,
                            .secondShape = true}});
        }
        if (command == QLatin1String("error")) {
            DbError e;
            e.category = DbError::Category::Syntax;
            e.sqlState = QStringLiteral("42601");
            e.message = QStringLiteral("syntax error");
            e.position = argument.toInt();
            return fail(e);
        }
        if (command == QLatin1String("sleep")) {
            QElapsedTimer t;
            t.start();
            while (t.elapsed() < argument.toLongLong()) {
                if (*m_cancel)
                    return fail(DbError::Category::Cancelled,
                                QStringLiteral("canceling statement due to user request"));
                QThread::sleep(std::chrono::milliseconds(2));
            }
            return cursor({{}});
        }
        return fail(DbError::Category::Syntax, QStringLiteral("unknown fake command"));
    }

    Result<void> setAutocommit(bool) override { return {}; }
    TxState txState() const override { return TxState::Idle; }
    QList<ServerMessage> takeMessages() override { return std::exchange(m_messages, {}); }

private:
    static Result<std::unique_ptr<IResultCursor>> cursor(QList<FakeResult> results)
    {
        return std::unique_ptr<IResultCursor>(std::make_unique<FakeCursor>(std::move(results)));
    }

    bool m_open = false;
    std::shared_ptr<std::atomic<bool>> m_cancel = std::make_shared<std::atomic<bool>>(false);
    QList<ServerMessage> m_messages;
};

} // namespace

std::unique_ptr<IConnection> FakeDriver::createConnection() const
{
    return std::make_unique<FakeConnection>();
}

} // namespace owl::test
