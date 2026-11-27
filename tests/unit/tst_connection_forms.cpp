// SPDX-License-Identifier: GPL-3.0-or-later
// Connection forms and URLs (decision 166): form values ⇄ ConnectionParams, validation, hidden
// fields, JSON round trips, SSL mode names, and every driver's form and URL parser.
#include "mysqldriver.h"
#include "owl/core/driverregistry.h"
#include "pgdriver.h"
#include "sqlitedriver.h"

#include <QDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace owl;

class TestConnectionForms : public QObject
{
    Q_OBJECT

    DriverRegistry m_registry;

private slots:
    void initTestCase()
    {
        QVERIFY(m_registry.add(std::make_shared<postgres::PgDriver>()));
        QVERIFY(m_registry.add(std::make_shared<mysql::MySqlDriver>()));
        QVERIFY(m_registry.add(std::make_shared<sqlite::SqliteDriver>()));
    }

    void sslModeNames()
    {
        QCOMPARE(sslModeName(SslOptions::Mode::VerifyFull), u"verify-full"_s);
        QCOMPARE(sslModeFromName(u"verify-ca"), SslOptions::Mode::VerifyCa);
        QCOMPARE(sslModeFromName(u"REQUIRED"), SslOptions::Mode::Require); // MySQL spelling
        QCOMPARE(sslModeFromName(u"VERIFY_IDENTITY"), SslOptions::Mode::VerifyFull);
        QVERIFY(!sslModeFromName(u"sometimes"));
    }

    void formRoundTrip()
    {
        const ConnectionFormSchema schema = m_registry.driver(u"postgres"_s)->connectionForm();
        QVERIFY(schema.field(u"host"));
        QCOMPARE(schema.field(u"port")->defaultValue.toInt(), 5432);
        QCOMPARE(schema.field(u"password")->type, FormField::Type::Password);

        ConnectionParams p;
        p.driverId = u"postgres"_s;
        p.host = u"db.internal"_s;
        p.port = 6432;
        p.user = u"app"_s;
        p.database = u"app_db"_s;
        p.ssl.mode = SslOptions::Mode::VerifyFull;
        p.ssl.caFile = u"/etc/ca.pem"_s;
        p.driverOptions.insert(u"options"_s, u"-c search_path=app"_s);
        const QVariantMap values = formValues(schema, p);
        QCOMPARE(values.value(u"ssl.mode"_s).toString(), u"verify-full"_s);
        QCOMPARE(values.value(u"transport"_s).toString(), u"tcp"_s);
        QCOMPARE(values.value(u"option.options"_s).toString(), u"-c search_path=app"_s);

        const Result<ConnectionParams> back = paramsFromForm(schema, u"postgres"_s, values);
        QVERIFY2(back, qPrintable(back ? QString() : back.error().message));
        QCOMPARE(*back, p);
    }

    void defaultsFillTheForm()
    {
        const ConnectionFormSchema schema = m_registry.driver(u"mysql"_s)->connectionForm();
        const QVariantMap values = formValues(schema, ConnectionParams{});
        QCOMPARE(values.value(u"host"_s).toString(), u"localhost"_s);
        QCOMPARE(values.value(u"port"_s).toInt(), 3306);
        QCOMPARE(values.value(u"ssl.mode"_s).toString(), u"prefer"_s);
    }

    void validation()
    {
        const ConnectionFormSchema schema = m_registry.driver(u"postgres"_s)->connectionForm();
        QVariantMap values = formValues(schema, ConnectionParams{});
        values.insert(u"host"_s, u"  "_s);
        values.insert(u"port"_s, u"70000"_s);
        values.insert(u"ssl.mode"_s, u"sometimes"_s);
        QList<FieldError> errors;
        QVERIFY(!paramsFromForm(schema, u"postgres"_s, values, &errors));
        QStringList keys;
        for (const FieldError &e : errors)
            keys << e.key;
        QCOMPARE(keys, (QStringList{u"host"_s, u"port"_s, u"user"_s, u"ssl.mode"_s}));
    }

    void hiddenFieldsAreSkipped()
    {
        const ConnectionFormSchema schema = m_registry.driver(u"postgres"_s)->connectionForm();
        QVariantMap values = formValues(schema, ConnectionParams{});
        values.insert(u"transport"_s, u"socket"_s);
        values.insert(u"host"_s, QString()); // required for TCP, hidden for sockets
        values.insert(u"user"_s, u"me"_s);
        const Result<ConnectionParams> p = paramsFromForm(schema, u"postgres"_s, values);
        QVERIFY2(p, qPrintable(p ? QString() : p.error().message));
        QCOMPARE(p->transport, ConnectionParams::Transport::Socket);
        QCOMPARE(p->socketPath, u"/var/run/postgresql"_s);
        QVERIFY(p->host.isEmpty());
    }

    void jsonNeverHoldsThePassword()
    {
        ConnectionParams p;
        p.driverId = u"mysql"_s;
        p.transport = ConnectionParams::Transport::Socket;
        p.socketPath = u"/run/mysqld.sock"_s;
        p.user = u"root"_s;
        p.password = u"secret"_s;
        p.connectTimeout = std::chrono::seconds(5);
        p.driverOptions.insert(u"initCommand"_s, u"SET x = 1"_s);
        const QJsonObject json = toJson(p);
        QVERIFY(!json.contains(u"password"));
        ConnectionParams back = connectionParamsFromJson(json, u"mysql"_s);
        p.password.clear();
        QCOMPARE(back, p);
    }

    void postgresUrls()
    {
        const Result<ConnectionParams> uri = m_registry.parseUrl(
            u"postgresql://app:s%40cret@db.example.com:6432/app_db?sslmode=verify-full"
            u"&application_name=report&target_session_attrs=read-write"_s);
        QVERIFY2(uri, qPrintable(uri ? QString() : uri.error().message));
        QCOMPARE(uri->driverId, u"postgres"_s);
        QCOMPARE(uri->host, u"db.example.com"_s);
        QCOMPARE(uri->port, 6432);
        QCOMPARE(uri->user, u"app"_s);
        QCOMPARE(uri->password, u"s@cret"_s);
        QCOMPARE(uri->database, u"app_db"_s);
        QCOMPARE(uri->ssl.mode, SslOptions::Mode::VerifyFull);
        QCOMPARE(uri->applicationName, u"report"_s);
        QCOMPARE(uri->driverOptions.value(u"target_session_attrs"_s).toString(), u"read-write"_s);

        const Result<ConnectionParams> keywords
            = m_registry.parseUrl(u"host=/var/run/postgresql dbname=app user=me"_s);
        QVERIFY(keywords);
        QCOMPARE(keywords->transport, ConnectionParams::Transport::Socket);
        QCOMPARE(keywords->socketPath, u"/var/run/postgresql"_s);

        const Result<ConnectionParams> broken
            = m_registry.parseUrl(u"postgresql://host:notaport/"_s);
        QVERIFY(!broken);
        QCOMPARE(broken.error().category, DbError::Category::Other); // recognised, but invalid
    }

    void mysqlUrls()
    {
        const Result<ConnectionParams> p = m_registry.parseUrl(
            u"mariadb://root:pa%3Ass@127.0.0.1:3307/shop?ssl-mode=REQUIRED&ssl-ca=/ca.pem"_s);
        QVERIFY2(p, qPrintable(p ? QString() : p.error().message));
        QCOMPARE(p->driverId, u"mysql"_s);
        QCOMPARE(p->password, u"pa:ss"_s);
        QCOMPARE(p->port, 3307);
        QCOMPARE(p->database, u"shop"_s);
        QCOMPARE(p->ssl.mode, SslOptions::Mode::Require);
        QCOMPARE(p->ssl.caFile, u"/ca.pem"_s);

        const Result<ConnectionParams> socket
            = m_registry.parseUrl(u"mysql://me@localhost/db?socket=/run/mysqld/mysqld.sock"_s);
        QCOMPARE(socket->transport, ConnectionParams::Transport::Socket);
        QCOMPARE(socket->socketPath, u"/run/mysqld/mysqld.sock"_s);

        QVERIFY(!m_registry.parseUrl(u"mysql://me@host/db?ssl-mode=SOMETIMES"_s));
    }

    void sqliteUrls()
    {
        QCOMPARE(m_registry.parseUrl(u"sqlite:///data/app.db"_s)->database, u"/data/app.db"_s);
        QCOMPARE(m_registry.parseUrl(u"file:app.db?mode=ro"_s)->database, u"file:app.db?mode=ro"_s);
        QCOMPARE(m_registry.parseUrl(u"~/notes.sqlite3"_s)->database,
                 QDir::homePath() + u"/notes.sqlite3"_s);
        const Result<ConnectionParams> none = m_registry.parseUrl(u"ftp://example.com/x"_s);
        QVERIFY(!none);
        QCOMPARE(none.error().category, DbError::Category::Unsupported);
        QVERIFY(!m_registry.parseUrl(u"just some text"_s));
    }

    void sqliteForm()
    {
        const ConnectionFormSchema schema = m_registry.driver(u"sqlite"_s)->connectionForm();
        QVariantMap values = formValues(schema, ConnectionParams{});
        QVERIFY(!paramsFromForm(schema, u"sqlite"_s, values)); // the file is required
        values.insert(u"database"_s, u"/tmp/x.db"_s);
        values.insert(u"option.create"_s, true);
        const Result<ConnectionParams> p = paramsFromForm(schema, u"sqlite"_s, values);
        QVERIFY(p);
        QVERIFY(p->driverOptions.value(u"create"_s).toBool());
        QVERIFY(!p->driverOptions.value(u"readOnly"_s).toBool());
    }
};

QTEST_GUILESS_MAIN(TestConnectionForms)
#include "tst_connection_forms.moc"
