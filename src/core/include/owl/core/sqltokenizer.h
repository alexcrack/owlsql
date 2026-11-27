// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QStringView>

namespace owl {

// A lexical SQL token: enough for statement splitting, classification, placeholders and (later)
// completion context — not a parser. Positions are UTF-16 offsets into the scanned text.
struct SqlToken
{
    enum class Type : quint8 {
        Whitespace,
        LineComment, // -- … or # … (MySQL)
        BlockComment, // /* … */
        Word, // keyword or unquoted identifier
        QuotedIdentifier, // "x", `x`, [x]
        String, // '…', E'…', N'…', X'…'
        DollarString, // $tag$ … $tag$ (PostgreSQL)
        Number,
        Placeholder, // ?, ?1, $1, :name, @name, $name
        Semicolon,
        OpenParen,
        CloseParen,
        Symbol, // any other punctuation / operator character
    };

    Type type = Type::Symbol;
    qsizetype start = 0;
    qsizetype length = 0;
    bool unterminated = false; // string/comment/identifier running to the end of input

    QStringView text(QStringView sql) const { return sql.mid(start, length); }
    bool isTrivia() const
    {
        return type == Type::Whitespace || type == Type::LineComment || type == Type::BlockComment;
    }
};

struct SqlLexerOptions
{
    bool backtickIdentifiers = false; // `x` (MySQL, SQLite)
    bool bracketIdentifiers = false; // [x] (SQLite, SQL Server)
    bool dollarQuotes = false; // $tag$…$tag$ (PostgreSQL)
    bool nestedBlockComments = false; // /* /* */ */ (PostgreSQL)
    bool hashComments = false; // # … (MySQL)
    bool backslashEscapes = false; // 'it\'s' (MySQL default)
    bool doubleQuotedStrings = false; // "x" is a string, not an identifier (MySQL)
    bool dashCommentNeedsSpace = false; // "-- x" is a comment, "--1" is not (MySQL)
    // Placeholder forms recognised outside strings and comments.
    bool questionPlaceholders = true; // ?   and ?NNN (SQLite)
    bool dollarNumberPlaceholders = false; // $1 (PostgreSQL)
    bool namedPlaceholders = false; // :name, @name, $name (SQLite)
};

QList<SqlToken> tokenizeSql(QStringView sql, const SqlLexerOptions &options);

} // namespace owl
