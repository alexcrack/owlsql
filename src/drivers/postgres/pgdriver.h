// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "owl/core/driver.h"
#include "pgdialect.h"

namespace owl::postgres {

// PostgreSQL over libpq (specs/driver-api.md §11). ConnectionParams map to libpq keywords:
// host / port (or socketPath as host for Unix sockets), user, password, database → dbname,
// ssl → sslmode / sslrootcert / sslcert / sslkey, applicationName, connectTimeout.
// driverOptions: any other libpq connection keyword ("options", "target_session_attrs",
// "sslnegotiation", …), passed through as text; unknown keys fail the connection.
class PgDriver : public IDriver
{
public:
    QString id() const override { return QStringLiteral("postgres"); }
    QString displayName() const override { return QStringLiteral("PostgreSQL"); }
    Capabilities capabilities() const override;
    const IDialect &dialect() const override { return m_dialect; }
    std::unique_ptr<IConnection> createConnection() const override;

private:
    PgDialect m_dialect;
};

} // namespace owl::postgres
