// SPDX-License-Identifier: GPL-3.0-or-later
// Database servers for integration and conformance tests (decisions 125, 151).
//
// Each test reads a URL from an environment variable, e.g.
//   OWL_TEST_PG_URL=postgresql://postgres:owlsql@localhost:5432/postgres
//   OWL_TEST_MYSQL_URL=mysql://root:owlsql@localhost:3306/owl   (also mariadb://)
// with an optional ?sslmode=disable|prefer|require. ctest runs the same test against further
// servers by naming another variable in OWL_TEST_URL_VAR (OWL_TEST_PG13_URL,
// OWL_TEST_MYSQL57_URL…). Without a URL the test skips — unless OWL_REQUIRE_INTEGRATION_TESTS is
// set, then it fails.
#pragma once

#include "owl/core/driver.h"

#include <QUrl>
#include <QUrlQuery>

namespace owl::test {

struct TestServer
{
    ConnectionParams params;
    QString urlVariable;
    QString skipReason; // no server configured: skip
    QString error; // a server is required but not configured / the URL is invalid: fail
};

// `defaultVariable`: the URL variable when OWL_TEST_URL_VAR is not set; `driverId`: "postgres"
// or "mysql".
inline TestServer testServer(const char *defaultVariable, const QString &driverId)
{
    TestServer s;
    s.urlVariable = qEnvironmentVariableIsSet("OWL_TEST_URL_VAR")
        ? qEnvironmentVariable("OWL_TEST_URL_VAR")
        : QString::fromLatin1(defaultVariable);
    const QString text = qEnvironmentVariable(s.urlVariable.toLocal8Bit().constData());
    if (text.isEmpty()) {
        const QString what = s.urlVariable + QStringLiteral(" not set");
        if (qEnvironmentVariableIsSet("OWL_REQUIRE_INTEGRATION_TESTS"))
            s.error = what + QStringLiteral(" but OWL_REQUIRE_INTEGRATION_TESTS is");
        else
            s.skipReason = what + QStringLiteral(" — no server to test against");
        return s;
    }
    const QUrl url(text);
    const bool postgres = driverId == QLatin1String("postgres");
    const QStringList schemes = postgres
        ? QStringList{QStringLiteral("postgresql"), QStringLiteral("postgres")}
        : QStringList{QStringLiteral("mysql"), QStringLiteral("mariadb")};
    if (!url.isValid() || !schemes.contains(url.scheme())) {
        s.error = s.urlVariable + QStringLiteral(" is not a %1:// URL").arg(schemes.first());
        return s;
    }
    ConnectionParams &p = s.params;
    p.driverId = driverId;
    p.host = url.host();
    p.port = static_cast<quint16>(url.port(postgres ? 5432 : 3306));
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
