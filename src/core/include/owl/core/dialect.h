// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/sqltokenizer.h"
#include "owl/core/value.h"

#include <QList>
#include <QString>
#include <QStringView>

namespace owl {

// What a statement does — drives the production safety guards (specs/driver-api.md §9).
enum class StatementClass : quint8 {
    Read, // SELECT, VALUES, SHOW, EXPLAIN, read-only PRAGMA…
    Write, // INSERT, MERGE, bounded UPDATE/DELETE, CALL, data-modifying CTEs…
    WriteUnbounded, // UPDATE/DELETE without a top-level WHERE, TRUNCATE
    Ddl, // CREATE, ALTER, DROP, GRANT…
    Tx, // BEGIN, COMMIT, ROLLBACK, SAVEPOINT…
    Other, // SET, ATTACH, ANALYZE, unknown…
};

// A statement's range inside a script (UTF-16 offsets), without the terminating ';'.
struct StatementSpan
{
    qsizetype start = 0;
    qsizetype length = 0;

    QStringView in(QStringView script) const { return script.mid(start, length); }
    bool operator==(const StatementSpan &) const = default;
};

struct Placeholder
{
    qsizetype start = 0;
    qsizetype length = 0;
    QString name; // "1" for ?1/$1, "name" for :name/@name/$name, empty for a bare ?

    bool operator==(const Placeholder &) const = default;
};

// SQL knowledge of one engine. Pure and thread-safe: no I/O, callable from any thread
// (the editor uses it for splitting and completion). docs/specs/driver-api.md §5.
// SQL generation (DDL diffs, row changes, browse queries) is added in later milestones.
class IDialect
{
public:
    virtual ~IDialect() = default;
    virtual QString quoteIdentifier(QStringView name) const = 0;
    virtual QString quoteLiteral(const Value &value) const = 0;
    virtual QList<StatementSpan> split(QStringView script) const = 0;
    virtual StatementClass classify(QStringView statement) const = 0;
    virtual QList<Placeholder> placeholders(QStringView statement) const = 0;
    virtual QList<SqlToken> tokenize(QStringView sql) const = 0;
};

// Shared implementation driven by lexer options; engines subclass it and override what differs.
class SqlDialect : public IDialect
{
public:
    explicit SqlDialect(SqlLexerOptions options)
        : m_options(options)
    { }

    QString quoteIdentifier(QStringView name) const override;
    QString quoteLiteral(const Value &value) const override;
    QList<StatementSpan> split(QStringView script) const override;
    StatementClass classify(QStringView statement) const override;
    QList<Placeholder> placeholders(QStringView statement) const override;
    QList<SqlToken> tokenize(QStringView sql) const override;

protected:
    // Statements whose body contains ';' inside BEGIN…END (triggers, BEGIN ATOMIC functions).
    virtual bool opensCompoundBody(const QList<QStringView> &leadingWords) const;
    virtual QString quoteBytes(const QByteArray &bytes) const; // X'hex' by default

    const SqlLexerOptions &options() const { return m_options; }

private:
    SqlLexerOptions m_options;
};

QString statementClassName(StatementClass c);

} // namespace owl
