// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/dialect.h"

#include <cmath>

namespace owl {

namespace {

bool is(QStringView word, QLatin1StringView keyword)
{
    return word.compare(keyword, Qt::CaseInsensitive) == 0;
}

bool isAnyOf(QStringView word, std::initializer_list<QLatin1StringView> keywords)
{
    for (const QLatin1StringView k : keywords) {
        if (is(word, k))
            return true;
    }
    return false;
}

// Non-trivia tokens of one statement, with their parenthesis depth.
struct Significant
{
    SqlToken token;
    int depth = 0;
};

QList<Significant> significant(QStringView sql, const QList<SqlToken> &tokens)
{
    Q_UNUSED(sql)
    QList<Significant> out;
    int depth = 0;
    for (const SqlToken &t : tokens) {
        if (t.isTrivia())
            continue;
        if (t.type == SqlToken::Type::CloseParen)
            depth = qMax(0, depth - 1);
        out.append({.token = t, .depth = depth});
        if (t.type == SqlToken::Type::OpenParen)
            ++depth;
    }
    return out;
}

bool hasTopLevelWord(QStringView sql, const QList<Significant> &tokens, qsizetype from,
                     QLatin1StringView word)
{
    for (qsizetype i = from; i < tokens.size(); ++i) {
        const Significant &s = tokens.at(i);
        if (s.depth == 0 && s.token.type == SqlToken::Type::Word && is(s.token.text(sql), word))
            return true;
    }
    return false;
}

bool hasTopLevelSymbol(QStringView sql, const QList<Significant> &tokens, QChar symbol)
{
    for (const Significant &s : tokens) {
        if (s.depth == 0 && s.token.type == SqlToken::Type::Symbol
            && s.token.text(sql) == QStringView(&symbol, 1))
            return true;
    }
    return false;
}

StatementClass classifyFrom(QStringView sql, const QList<Significant> &tokens, qsizetype index);

// UPDATE/DELETE at `index`: bounded when a WHERE follows at the same nesting level.
StatementClass updateOrDelete(QStringView sql, const QList<Significant> &tokens, qsizetype index)
{
    const int depth = tokens.at(index).depth;
    for (qsizetype i = index + 1; i < tokens.size(); ++i) {
        const Significant &s = tokens.at(i);
        if (s.depth < depth)
            break;
        if (s.depth == depth && s.token.type == SqlToken::Type::Word
            && is(s.token.text(sql), QLatin1StringView("WHERE")))
            return StatementClass::Write;
    }
    return StatementClass::WriteUnbounded;
}

// WITH … main-statement: data-modifying CTEs make it a write; otherwise classify the main part.
StatementClass withStatement(QStringView sql, const QList<Significant> &tokens, qsizetype index)
{
    const int depth = tokens.at(index).depth;
    bool modifyingCte = false;
    for (qsizetype i = index + 1; i < tokens.size(); ++i) {
        const Significant &s = tokens.at(i);
        if (s.token.type != SqlToken::Type::Word)
            continue;
        const QStringView w = s.token.text(sql);
        if (s.depth > depth) {
            if (isAnyOf(w,
                        {QLatin1StringView("INSERT"), QLatin1StringView("UPDATE"),
                         QLatin1StringView("DELETE"), QLatin1StringView("MERGE")}))
                modifyingCte = true;
            continue;
        }
        if (isAnyOf(w,
                    {QLatin1StringView("SELECT"), QLatin1StringView("INSERT"),
                     QLatin1StringView("UPDATE"), QLatin1StringView("DELETE"),
                     QLatin1StringView("MERGE"), QLatin1StringView("VALUES"),
                     QLatin1StringView("REPLACE"), QLatin1StringView("TABLE")})) {
            const StatementClass main = classifyFrom(sql, tokens, i);
            if (main == StatementClass::Read && modifyingCte)
                return StatementClass::Write;
            return main;
        }
    }
    return modifyingCte ? StatementClass::Write : StatementClass::Read;
}

StatementClass classifyFrom(QStringView sql, const QList<Significant> &tokens, qsizetype index)
{
    // Skip leading parentheses: "(SELECT …) UNION …"
    while (index < tokens.size() && tokens.at(index).token.type == SqlToken::Type::OpenParen)
        ++index;
    if (index >= tokens.size() || tokens.at(index).token.type != SqlToken::Type::Word)
        return StatementClass::Other;
    const QStringView w = tokens.at(index).token.text(sql);
    using L = QLatin1StringView;

    if (isAnyOf(w,
                {L("SELECT"), L("VALUES"), L("SHOW"), L("DESCRIBE"), L("DESC"), L("TABLE"),
                 L("FETCH")}))
        return StatementClass::Read;
    if (is(w, L("WITH")))
        return withStatement(sql, tokens, index);
    if (is(w, L("EXPLAIN"))) {
        // Plain EXPLAIN only plans; EXPLAIN ANALYZE (also inside an option list) executes.
        const int depth = tokens.at(index).depth;
        bool analyze = false;
        qsizetype i = index + 1;
        for (; i < tokens.size(); ++i) {
            const Significant &s = tokens.at(i);
            const bool word = s.token.type == SqlToken::Type::Word;
            const QStringView text = s.token.text(sql);
            if (word && isAnyOf(text, {L("ANALYZE"), L("ANALYSE")})) {
                analyze = true;
                continue;
            }
            if (s.depth > depth || s.token.type == SqlToken::Type::OpenParen
                || s.token.type == SqlToken::Type::CloseParen)
                continue; // PostgreSQL option list: (ANALYZE, BUFFERS, FORMAT JSON)
            if (word
                && isAnyOf(text,
                           {L("QUERY"), L("PLAN"), L("VERBOSE"), L("FORMAT"), L("JSON"), L("TEXT"),
                            L("XML"), L("YAML"), L("TREE"), L("TRADITIONAL"), L("EXTENDED"),
                            L("PARTITIONS")}))
                continue;
            if (s.token.type == SqlToken::Type::Symbol && text == u"=")
                continue; // MySQL FORMAT=JSON
            break;
        }
        if (i >= tokens.size())
            return StatementClass::Read;
        return analyze ? classifyFrom(sql, tokens, i) : StatementClass::Read;
    }
    if (isAnyOf(w,
                {L("INSERT"), L("REPLACE"), L("UPSERT"), L("MERGE"), L("CALL"), L("DO"), L("EXEC"),
                 L("EXECUTE"), L("VACUUM"), L("REINDEX"), L("CLUSTER"), L("REFRESH"), L("LOAD"),
                 L("IMPORT")}))
        return StatementClass::Write;
    if (is(w, L("COPY")))
        return hasTopLevelWord(sql, tokens, index + 1, L("FROM")) ? StatementClass::Write
                                                                  : StatementClass::Read;
    if (isAnyOf(w, {L("UPDATE"), L("DELETE")}))
        return updateOrDelete(sql, tokens, index);
    if (is(w, L("TRUNCATE")))
        return StatementClass::WriteUnbounded;
    if (isAnyOf(w,
                {L("CREATE"), L("ALTER"), L("DROP"), L("RENAME"), L("COMMENT"), L("GRANT"),
                 L("REVOKE"), L("SECURITY")}))
        return StatementClass::Ddl;
    if (isAnyOf(w,
                {L("BEGIN"), L("START"), L("COMMIT"), L("END"), L("ROLLBACK"), L("SAVEPOINT"),
                 L("RELEASE"), L("ABORT"), L("PREPARE")})) {
        // PREPARE TRANSACTION is Tx; PREPARE name AS … is a statement definition.
        if (is(w, L("PREPARE")))
            return hasTopLevelWord(sql, tokens, index + 1, L("TRANSACTION"))
                ? StatementClass::Tx
                : StatementClass::Other;
        return StatementClass::Tx;
    }
    if (is(w, L("PRAGMA")))
        return hasTopLevelSymbol(sql, tokens, u'=') ? StatementClass::Other : StatementClass::Read;
    return StatementClass::Other;
}

QString quoteString(QStringView text)
{
    QString out;
    out.reserve(text.size() + 2);
    out += u'\'';
    for (const QChar c : text) {
        if (c == u'\'')
            out += u'\'';
        out += c;
    }
    out += u'\'';
    return out;
}

} // namespace

QString statementClassName(StatementClass c)
{
    switch (c) {
    case StatementClass::Read:
        return QStringLiteral("read");
    case StatementClass::Write:
        return QStringLiteral("write");
    case StatementClass::WriteUnbounded:
        return QStringLiteral("write-unbounded");
    case StatementClass::Ddl:
        return QStringLiteral("ddl");
    case StatementClass::Tx:
        return QStringLiteral("tx");
    case StatementClass::Other:
        return QStringLiteral("other");
    }
    return {};
}

QList<SqlToken> SqlDialect::tokenize(QStringView sql) const
{
    return tokenizeSql(sql, m_options);
}

QString SqlDialect::quoteIdentifier(QStringView name) const
{
    QString out;
    out.reserve(name.size() + 2);
    out += u'"';
    for (const QChar c : name) {
        if (c == u'"')
            out += u'"';
        out += c;
    }
    out += u'"';
    return out;
}

QString SqlDialect::quoteBytes(const QByteArray &bytes) const
{
    return QStringLiteral("X'") + QString::fromLatin1(bytes.toHex()) + u'\'';
}

QString SqlDialect::quoteLiteral(const Value &value) const
{
    switch (value.kind()) {
    case ValueKind::Null:
        return QStringLiteral("NULL");
    case ValueKind::Bool:
        return value.toBool() ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    case ValueKind::Int:
        return QString::number(value.toInt());
    case ValueKind::UInt:
        return QString::number(value.toUInt());
    case ValueKind::Double: {
        const double d = value.toDouble();
        if (std::isnan(d))
            return QStringLiteral("NULL");
        if (std::isinf(d))
            return d > 0 ? QStringLiteral("9e999") : QStringLiteral("-9e999");
        return QString::number(d, 'g', 17);
    }
    case ValueKind::Decimal:
        return value.toText();
    case ValueKind::Bytes:
        return quoteBytes(value.toBytes());
    case ValueKind::Uuid:
        return quoteString(value.toUuid().toString(QUuid::WithoutBraces));
    case ValueKind::Array: {
        QStringList parts; // PostgreSQL-style array literal as text; engines override
        for (const Value &v : value.toArray())
            parts << (v.isNull() ? QStringLiteral("NULL") : v.toText());
        return quoteString(QString(u'{' + parts.join(u',') + u'}'));
    }
    default:
        return quoteString(value.toText());
    }
}

bool SqlDialect::opensCompoundBody(const QList<QStringView> &leadingWords) const
{
    // CREATE [TEMP|TEMPORARY] TRIGGER … BEGIN … END
    qsizetype i = 0;
    if (i < leadingWords.size() && is(leadingWords.at(i), QLatin1StringView("CREATE")))
        ++i;
    else
        return false;
    if (i < leadingWords.size()
        && isAnyOf(leadingWords.at(i), {QLatin1StringView("TEMP"), QLatin1StringView("TEMPORARY")}))
        ++i;
    return i < leadingWords.size() && is(leadingWords.at(i), QLatin1StringView("TRIGGER"));
}

QList<StatementSpan> SqlDialect::split(QStringView script) const
{
    const QList<SqlToken> tokens = tokenize(script);
    QList<StatementSpan> spans;

    qsizetype first = -1; // first significant token of the current statement
    qsizetype last = -1;
    QList<QStringView> leadingWords;
    bool compound = false;
    bool atomicSeen = false;
    int blockDepth = 0;

    const auto flush = [&] {
        if (first >= 0) {
            const qsizetype start = tokens.at(first).start;
            const qsizetype end = tokens.at(last).start + tokens.at(last).length;
            spans.append({.start = start, .length = end - start});
        }
        first = last = -1;
        leadingWords.clear();
        compound = atomicSeen = false;
        blockDepth = 0;
    };

    for (qsizetype i = 0; i < tokens.size(); ++i) {
        const SqlToken &t = tokens.at(i);
        if (t.type == SqlToken::Type::Semicolon && blockDepth == 0) {
            flush();
            continue;
        }
        if (t.type == SqlToken::Type::Whitespace)
            continue;
        if (t.isTrivia()) {
            if (first < 0)
                continue; // comments before a statement belong to no statement
            last = i;
            continue;
        }
        if (first < 0)
            first = i;
        last = i;

        if (t.type != SqlToken::Type::Word)
            continue;
        const QStringView word = t.text(script);
        if (leadingWords.size() < 4) {
            leadingWords.append(word);
            if (!compound && opensCompoundBody(leadingWords))
                compound = true;
        }
        const bool beginAtomic = atomicSeen && is(word, QLatin1StringView("ATOMIC"));
        atomicSeen = is(word, QLatin1StringView("BEGIN"));
        if (beginAtomic && !compound) {
            compound = true; // BEGIN ATOMIC … END (SQL-standard function body)
            ++blockDepth; // for the BEGIN just before, which was not counted
            continue;
        }
        if (!compound)
            continue;
        if (is(word, QLatin1StringView("BEGIN")) || is(word, QLatin1StringView("CASE")))
            ++blockDepth;
        else if (is(word, QLatin1StringView("END")) && blockDepth > 0)
            --blockDepth;
    }
    flush();
    return spans;
}

StatementClass SqlDialect::classify(QStringView statement) const
{
    const QList<SqlToken> tokens = tokenize(statement);
    return classifyFrom(statement, significant(statement, tokens), 0);
}

QList<Placeholder> SqlDialect::placeholders(QStringView statement) const
{
    QList<Placeholder> out;
    const QList<SqlToken> tokens = tokenize(statement);
    for (const SqlToken &t : tokens) {
        if (t.type != SqlToken::Type::Placeholder)
            continue;
        const QStringView text = t.text(statement);
        out.append({.start = t.start, .length = t.length, .name = text.mid(1).toString()});
    }
    return out;
}

} // namespace owl
