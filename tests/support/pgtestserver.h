// SPDX-License-Identifier: GPL-3.0-or-later
// The PostgreSQL server for integration and conformance tests (decision 125).
//
// The URL comes from OWL_TEST_PG_URL, e.g. postgresql://postgres:owlsql@localhost:5432/postgres
// (query: ?sslmode=disable|prefer|require). ctest runs the same tests against further servers by
// naming another variable in OWL_TEST_PG_URL_VAR (OWL_TEST_PG13_URL on CI). Without a URL the
// tests skip — unless OWL_REQUIRE_INTEGRATION_TESTS is set, then they fail.
#pragma once

#include "owl/core/driver.h"

#include <QUrl>
#include <QUrlQuery>

namespace owl::test {

struct PgTestServer
{
    ConnectionParams params;
    QString urlVariable;
    QString skipReason; // no server configured: skip
    QString error; // a server is required but not configured / the URL is invalid: fail
};

inline PgTestServer pgTestServer()
{
    PgTestServer s;
    s.urlVariable = qEnvironmentVariableIsSet("OWL_TEST_PG_URL_VAR")
        ? qEnvironmentVariable("OWL_TEST_PG_URL_VAR")
        : QStringLiteral("OWL_TEST_PG_URL");
    const QString text = qEnvironmentVariable(s.urlVariable.toLocal8Bit().constData());
    if (text.isEmpty()) {
        const QString what = s.urlVariable + QStringLiteral(" not set");
        if (qEnvironmentVariableIsSet("OWL_REQUIRE_INTEGRATION_TESTS"))
            s.error = what + QStringLiteral(" but OWL_REQUIRE_INTEGRATION_TESTS is");
        else
            s.skipReason = what + QStringLiteral(" — no PostgreSQL server to test against");
        return s;
    }
    const QUrl url(text);
    if (!url.isValid()
        || (url.scheme() != QLatin1String("postgresql")
            && url.scheme() != QLatin1String("postgres"))) {
        s.error = s.urlVariable + QStringLiteral(" is not a postgresql:// URL");
        return s;
    }
    ConnectionParams &p = s.params;
    p.driverId = QStringLiteral("postgres");
    p.host = url.host();
    p.port = static_cast<quint16>(url.port(5432));
    p.user = url.userName();
    p.password = url.password();
    p.database = url.path().mid(1);
    p.connectTimeout = std::chrono::seconds(10);
    const QString sslmode = QUrlQuery(url).queryItemValue(QStringLiteral("sslmode"));
    if (sslmode == QLatin1String("disable"))
        p.ssl.mode = SslOptions::Mode::Disable;
    else if (sslmode == QLatin1String("require"))
        p.ssl.mode = SslOptions::Mode::Require;
    return s;
}

} // namespace owl::test
