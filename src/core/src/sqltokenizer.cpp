// SPDX-License-Identifier: GPL-3.0-or-later
#include "owl/core/sqltokenizer.h"

#include <optional>

namespace owl {

namespace {

bool isIdentStart(QChar c)
{
    return c.isLetter() || c == u'_' || c.unicode() > 127;
}

bool isIdentPart(QChar c)
{
    return isIdentStart(c) || c.isDigit() || c == u'$';
}

class Lexer
{
public:
    Lexer(QStringView sql, const SqlLexerOptions &options)
        : m_sql(sql)
        , m_o(options)
    { }

    QList<SqlToken> run()
    {
        QList<SqlToken> tokens;
        while (m_pos < m_sql.size())
            tokens.append(next());
        return tokens;
    }

private:
    QChar at(qsizetype i) const { return i < m_sql.size() ? m_sql.at(i) : QChar(); }

    SqlToken make(SqlToken::Type type, qsizetype start, bool unterminated = false) const
    {
        SqlToken t;
        t.type = type;
        t.start = start;
        t.length = m_pos - start;
        t.unterminated = unterminated;
        return t;
    }

    SqlToken next()
    {
        const qsizetype start = m_pos;
        const QChar c = at(m_pos);

        if (c.isSpace()) {
            while (m_pos < m_sql.size() && at(m_pos).isSpace())
                ++m_pos;
            return make(SqlToken::Type::Whitespace, start);
        }
        const bool dashComment = c == u'-' && at(m_pos + 1) == u'-'
            && (!m_o.dashCommentNeedsSpace || m_pos + 2 >= m_sql.size() || at(m_pos + 2).isSpace());
        if (dashComment || (c == u'#' && m_o.hashComments)) {
            while (m_pos < m_sql.size() && at(m_pos) != u'\n')
                ++m_pos;
            return make(SqlToken::Type::LineComment, start);
        }
        if (c == u'/' && at(m_pos + 1) == u'*')
            return blockComment(start);

        // Prefixed strings: E'..' N'..' X'..' B'..' U&'..'
        if ((c == u'E' || c == u'e' || c == u'N' || c == u'n' || c == u'X' || c == u'x' || c == u'B'
             || c == u'b')
            && at(m_pos + 1) == u'\'') {
            ++m_pos;
            return quoted(start, u'\'', SqlToken::Type::String,
                          m_o.backslashEscapes || c == u'E' || c == u'e');
        }
        if (c == u'\'')
            return quoted(start, u'\'', SqlToken::Type::String, m_o.backslashEscapes);
        if (c == u'"' && m_o.doubleQuotedStrings)
            return quoted(start, u'"', SqlToken::Type::String, m_o.backslashEscapes);
        if (c == u'"')
            return quoted(start, u'"', SqlToken::Type::QuotedIdentifier, false);
        if (c == u'`' && m_o.backtickIdentifiers)
            return quoted(start, u'`', SqlToken::Type::QuotedIdentifier, false);
        if (c == u'[' && m_o.bracketIdentifiers) {
            ++m_pos;
            while (m_pos < m_sql.size() && at(m_pos) != u']')
                ++m_pos;
            const bool unterminated = m_pos >= m_sql.size();
            if (!unterminated)
                ++m_pos;
            return make(SqlToken::Type::QuotedIdentifier, start, unterminated);
        }

        if (c == u'$') {
            if (m_o.dollarQuotes) {
                if (auto t = dollarString(start))
                    return *t;
            }
            if (m_o.dollarNumberPlaceholders && at(m_pos + 1).isDigit()) {
                ++m_pos;
                while (at(m_pos).isDigit())
                    ++m_pos;
                return make(SqlToken::Type::Placeholder, start);
            }
            if (m_o.namedPlaceholders && isIdentStart(at(m_pos + 1)))
                return namedPlaceholder(start);
        }
        if (c == u'?' && m_o.questionPlaceholders) {
            ++m_pos;
            while (at(m_pos).isDigit())
                ++m_pos;
            return make(SqlToken::Type::Placeholder, start);
        }
        if ((c == u':' || c == u'@') && m_o.namedPlaceholders && isIdentStart(at(m_pos + 1))
            && (c != u':' || start == 0 || at(start - 1) != u':')) // not the "::" cast
            return namedPlaceholder(start);

        if (c.isDigit() || (c == u'.' && at(m_pos + 1).isDigit()))
            return number(start);
        if (isIdentStart(c)) {
            while (m_pos < m_sql.size() && isIdentPart(at(m_pos)))
                ++m_pos;
            return make(SqlToken::Type::Word, start);
        }

        ++m_pos;
        if (c == u';')
            return make(SqlToken::Type::Semicolon, start);
        if (c == u'(')
            return make(SqlToken::Type::OpenParen, start);
        if (c == u')')
            return make(SqlToken::Type::CloseParen, start);
        return make(SqlToken::Type::Symbol, start);
    }

    SqlToken blockComment(qsizetype start)
    {
        m_pos += 2;
        int depth = 1;
        while (m_pos < m_sql.size()) {
            if (at(m_pos) == u'*' && at(m_pos + 1) == u'/') {
                m_pos += 2;
                if (--depth == 0)
                    return make(SqlToken::Type::BlockComment, start);
            } else if (m_o.nestedBlockComments && at(m_pos) == u'/' && at(m_pos + 1) == u'*') {
                m_pos += 2;
                ++depth;
            } else {
                ++m_pos;
            }
        }
        return make(SqlToken::Type::BlockComment, start, true);
    }

    // m_pos at the opening quote. A doubled quote is an escaped quote.
    SqlToken quoted(qsizetype start, QChar quote, SqlToken::Type type, bool backslash)
    {
        ++m_pos;
        while (m_pos < m_sql.size()) {
            const QChar c = at(m_pos);
            if (backslash && c == u'\\') {
                m_pos += 2;
                continue;
            }
            if (c == quote) {
                if (at(m_pos + 1) == quote) {
                    m_pos += 2;
                    continue;
                }
                ++m_pos;
                return make(type, start);
            }
            ++m_pos;
        }
        return make(type, start, true);
    }

    // $tag$ … $tag$ ; returns nullopt when this '$' does not open a dollar quote.
    std::optional<SqlToken> dollarString(qsizetype start)
    {
        qsizetype i = m_pos + 1;
        while (i < m_sql.size() && at(i) != u'$') {
            const bool tagChar = isIdentStart(at(i)) || (i > m_pos + 1 && at(i).isDigit());
            if (!tagChar)
                return std::nullopt;
            ++i;
        }
        if (i >= m_sql.size())
            return std::nullopt;
        const QStringView tag = m_sql.mid(m_pos, i - m_pos + 1); // "$tag$"
        const qsizetype close = m_sql.indexOf(tag, i + 1);
        if (close < 0) {
            m_pos = m_sql.size();
            return make(SqlToken::Type::DollarString, start, true);
        }
        m_pos = close + tag.size();
        return make(SqlToken::Type::DollarString, start);
    }

    SqlToken namedPlaceholder(qsizetype start)
    {
        ++m_pos;
        while (m_pos < m_sql.size() && (isIdentPart(at(m_pos)) && at(m_pos) != u'$'))
            ++m_pos;
        return make(SqlToken::Type::Placeholder, start);
    }

    SqlToken number(qsizetype start)
    {
        bool seenDot = false;
        bool seenExp = false;
        while (m_pos < m_sql.size()) {
            const QChar c = at(m_pos);
            const bool hexMarker
                = (c == u'x' || c == u'X') && m_pos == start + 1 && at(start) == u'0';
            const bool hexDigit = c.isLetter() && m_pos > start + 1
                && (at(start + 1) == u'x' || at(start + 1) == u'X');
            if (c.isDigit() || c == u'_' || hexMarker || hexDigit) {
                ++m_pos;
            } else if (c == u'.' && !seenDot && !seenExp) {
                seenDot = true;
                ++m_pos;
            } else if ((c == u'e' || c == u'E') && !seenExp
                       && (at(m_pos + 1).isDigit()
                           || ((at(m_pos + 1) == u'+' || at(m_pos + 1) == u'-')
                               && at(m_pos + 2).isDigit()))) {
                seenExp = true;
                m_pos += 2;
            } else {
                break;
            }
        }
        return make(SqlToken::Type::Number, start);
    }

    QStringView m_sql;
    const SqlLexerOptions &m_o;
    qsizetype m_pos = 0;
};

} // namespace

QList<SqlToken> tokenizeSql(QStringView sql, const SqlLexerOptions &options)
{
    return Lexer(sql, options).run();
}

} // namespace owl
