// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mysqldialect.h"
#include "owl/core/driver.h"

namespace owl::mysql {

// MySQL 5.7+ and MariaDB 10.6+ over MariaDB Connector/C (specs/driver-api.md §11).
// ConnectionParams: host / port, or socketPath for Unix sockets; database (optional: switch later
// with USE); ssl (Disable, Prefer = TLS when offered without verification, Require, VerifyCa /
// VerifyFull = certificate and host name verified). driverOptions: "initCommand" (SQL run after
// connecting).
class MySqlDriver : public IDriver
{
public:
    QString id() const override { return QStringLiteral("mysql"); }
    QString displayName() const override { return QStringLiteral("MySQL / MariaDB"); }
    Capabilities capabilities() const override;
    const IDialect &dialect() const override { return m_dialect; }
    std::unique_ptr<IConnection> createConnection() const override;
    ConnectionFormSchema connectionForm() const override;
    Result<ConnectionParams> parseUrl(const QString &url) const override;

private:
    MySqlDialect m_dialect;
};

} // namespace owl::mysql
