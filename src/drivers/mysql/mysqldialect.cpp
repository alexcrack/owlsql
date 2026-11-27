// SPDX-License-Identifier: GPL-3.0-or-later
#include "mysqldialect.h"

namespace owl::mysql {

namespace {

SqlLexerOptions mysqlOptions()
{
    SqlLexerOptions o;
    o.backtickIdentifiers = true;
    o.hashComments = true;
    o.backslashEscapes = true;
    o.doubleQuotedStrings = true;
    o.dashCommentNeedsSpace = true;
    o.questionPlaceholders = true;
    return o;
}

bool is(QStringView word, QLatin1StringView keyword)
{
    return word.compare(keyword, Qt::CaseInsensitive) == 0;
}

bool isWord(QStringView sql, const SqlToken &t, QLatin1StringView keyword)
{
    return t.type == SqlToken::Type::Word && is(t.text(sql), keyword);
}

// '' and \ escaping: valid whether or not NO_BACKSLASH_ESCAPES is set.
QString quoteString(QStringView text)
{
    QString out;
    out.reserve(text.size() + 2);
    out += u'\'';
    for (const QChar c : text) {
        if (c == u'\'')
            out += u'\'';
        else if (c == u'\\' || c == QChar(0))
            out += u'\\';
        out += c == QChar(0) ? QChar(u'0') : c;
    }
    out += u'\'';
    return out;
}

// True when only spaces/tabs precede `pos` on its line.
bool atLineStart(QStringView script, qsizetype pos)
{
    for (qsizetype i = pos - 1; i >= 0; --i) {
        const QChar c = script.at(i);
        if (c == u'\n')
            return true;
        if (c != u' ' && c != u'\t' && c != u'\r')
            return false;
    }
    return true;
}

} // namespace

MySqlDialect::MySqlDialect()
    : SqlDialect(mysqlOptions())
{ }

QString MySqlDialect::quoteIdentifier(QStringView name) const
{
    QString out;
    out.reserve(name.size() + 2);
    out += u'`';
    for (const QChar c : name) {
        if (c == u'`')
            out += u'`';
        out += c;
    }
    out += u'`';
    return out;
}

QString MySqlDialect::quoteLiteral(const Value &value) const
{
    switch (value.kind()) {
    case ValueKind::Null:
    case ValueKind::Bool:
    case ValueKind::Int:
    case ValueKind::UInt:
    case ValueKind::Double:
    case ValueKind::Decimal:
    case ValueKind::Bytes:
        return SqlDialect::quoteLiteral(value);
    case ValueKind::Uuid:
        return quoteString(value.toUuid().toString(QUuid::WithoutBraces));
    default:
        return quoteString(value.toText());
    }
}

QList<StatementSpan> MySqlDialect::split(QStringView script) const
{
    const QList<SqlToken> tokens = tokenize(script);
    QList<StatementSpan> spans;
    QString delimiter = QStringLiteral(";");

    qsizetype first = -1; // first significant token of the current statement
    qsizetype last = -1;
    int significantWords = 0;
    bool compound = false; // stored-program body: ';' inside BEGIN … END does not end it
    int blockDepth = 0;
    QStringView previousWord;

    const auto flush = [&] {
        if (first >= 0) {
            const qsizetype start = tokens.at(first).start;
            const qsizetype end = tokens.at(last).start + tokens.at(last).length;
            spans.append({.start = start, .length = end - start});
        }
        first = last = -1;
        significantWords = 0;
        compound = false;
        blockDepth = 0;
        previousWord = {};
    };
    const auto nextWord = [&](qsizetype i) -> QStringView {
        for (qsizetype j = i + 1; j < tokens.size(); ++j) {
            if (tokens.at(j).isTrivia())
                continue;
            return tokens.at(j).type == SqlToken::Type::Word ? tokens.at(j).text(script)
                                                             : QStringView();
        }
        return {};
    };

    for (qsizetype i = 0; i < tokens.size(); ++i) {
        const SqlToken &t = tokens.at(i);

        // Client command "DELIMITER x" on a line of its own, between statements.
        if (first < 0 && isWord(script, t, QLatin1StringView("DELIMITER"))
            && atLineStart(script, t.start)) {
            qsizetype pos = t.start + t.length;
            while (pos < script.size() && (script.at(pos) == u' ' || script.at(pos) == u'\t'))
                ++pos;
            qsizetype end = pos;
            while (end < script.size() && !script.at(end).isSpace())
                ++end;
            if (end > pos)
                delimiter = script.mid(pos, end - pos).toString();
            while (end < script.size() && script.at(end) != u'\n')
                ++end;
            while (i + 1 < tokens.size() && tokens.at(i + 1).start < end)
                ++i;
            continue;
        }

        const bool code = t.type != SqlToken::Type::String
            && t.type != SqlToken::Type::QuotedIdentifier && !t.isTrivia();
        if (delimiter == u";") {
            if (t.type == SqlToken::Type::Semicolon && blockDepth == 0) {
                flush();
                continue;
            }
        } else if (code && script.mid(t.start).startsWith(delimiter)) {
            flush();
            while (i + 1 < tokens.size() && tokens.at(i + 1).start < t.start + delimiter.size())
                ++i;
            continue;
        }

        if (t.type == SqlToken::Type::Whitespace)
            continue;
        if (t.isTrivia()) {
            if (first >= 0)
                last = i;
            continue;
        }
        if (first < 0)
            first = i;
        last = i;

        if (t.type != SqlToken::Type::Word)
            continue;
        const QStringView word = t.text(script);
        ++significantWords;
        // CREATE [OR REPLACE] [DEFINER = …] [AGGREGATE] PROCEDURE|FUNCTION|TRIGGER|EVENT,
        // or MariaDB's anonymous BEGIN NOT ATOMIC block.
        if (!compound && significantWords <= 12
            && is(tokens.at(first).text(script), QLatin1StringView("CREATE"))
            && (is(word, QLatin1StringView("PROCEDURE")) || is(word, QLatin1StringView("FUNCTION"))
                || is(word, QLatin1StringView("TRIGGER")) || is(word, QLatin1StringView("EVENT"))))
            compound = true;
        if (!compound && significantWords == 2 && is(previousWord, QLatin1StringView("BEGIN"))
            && is(word, QLatin1StringView("NOT"))) {
            compound = true;
            blockDepth = 1;
        }
        if (compound) {
            // BEGIN, and CASE unless it is the CASE of END CASE, open a block.
            if (is(word, QLatin1StringView("BEGIN"))
                || (is(word, QLatin1StringView("CASE"))
                    && !is(previousWord, QLatin1StringView("END")))) {
                ++blockDepth;
            } else if (is(word, QLatin1StringView("END")) && blockDepth > 0) {
                const QStringView next = nextWord(i);
                // END IF / END LOOP / END WHILE / END REPEAT close blocks we do not count.
                if (!(is(next, QLatin1StringView("IF")) || is(next, QLatin1StringView("LOOP"))
                      || is(next, QLatin1StringView("WHILE"))
                      || is(next, QLatin1StringView("REPEAT"))))
                    --blockDepth;
            }
        }
        previousWord = word;
    }
    flush();
    return spans;
}

StatementClass MySqlDialect::classify(QStringView statement) const
{
    const StatementClass base = SqlDialect::classify(statement);
    if (base != StatementClass::Read)
        return base;
    // SELECT … INTO OUTFILE / DUMPFILE writes a file on the server (INTO @var does not).
    const QList<SqlToken> tokens = tokenize(statement);
    int depth = 0;
    bool into = false;
    for (const SqlToken &t : tokens) {
        if (t.isTrivia())
            continue;
        if (t.type == SqlToken::Type::OpenParen) {
            ++depth;
        } else if (t.type == SqlToken::Type::CloseParen) {
            depth = qMax(0, depth - 1);
        } else if (depth == 0 && into
                   && (isWord(statement, t, QLatin1StringView("OUTFILE"))
                       || isWord(statement, t, QLatin1StringView("DUMPFILE")))) {
            return StatementClass::Write;
        }
        into = depth == 0 && isWord(statement, t, QLatin1StringView("INTO"));
    }
    return base;
}

} // namespace owl::mysql
