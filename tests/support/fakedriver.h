// SPDX-License-Identifier: GPL-3.0-or-later
// Scriptable in-memory driver for tests (no database needed). Statements are commands:
//   rows N          2 columns (id int, name text), N rows
//   sleep MS        blocks up to MS ms; returns early with a Cancelled error when cancelled
//   error POS       Syntax error at character POS
//   notice TEXT     emits a Notice server message, no rows
//   update N        no rows, N affected rows
//   multi           two result sets (3 rows, then 2 rows with other columns)
// open() fails for host "fail" (Connection) and "auth" (Auth).
// catalog(): database "fake" → group "tables" → tables "a", "b"; a database named "fail" errors,
// paths containing "slow" take 300 ms.
#pragma once

#include "owl/core/driver.h"

namespace owl::test {

class FakeDriver : public IDriver
{
public:
    QString id() const override { return QStringLiteral("fake"); }
    QString displayName() const override { return QStringLiteral("Fake"); }
    Capabilities capabilities() const override
    {
        return {Capability::MultipleResultSets, Capability::ServerSideCancel};
    }
    const IDialect &dialect() const override { return m_dialect; }
    std::unique_ptr<IConnection> createConnection() const override;

private:
    SqlDialect m_dialect{SqlLexerOptions{}};
};

} // namespace owl::test
