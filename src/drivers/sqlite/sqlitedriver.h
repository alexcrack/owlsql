// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/driver.h"
#include "sqlitedialect.h"

namespace owl::sqlite {

// ConnectionParams for SQLite: `database` is the file path (or ":memory:", or a "file:" URI).
// driverOptions: "create" (bool, create a missing file), "readOnly" (bool, open read-only).
class SqliteDriver : public IDriver
{
public:
    QString id() const override { return QStringLiteral("sqlite"); }
    QString displayName() const override { return QStringLiteral("SQLite"); }
    Capabilities capabilities() const override;
    const IDialect &dialect() const override { return m_dialect; }
    std::unique_ptr<IConnection> createConnection() const override;

private:
    SqliteDialect m_dialect;
};

} // namespace owl::sqlite
