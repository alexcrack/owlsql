#include "sessiontreeitem.h"
#include <QDebug>

SessionTreeItem::SessionTreeItem(const QString &name, TreeItem *parentItem)
    : TreeItem(parentItem), m_sessionName(name)
{
    m_hostName = QString("127.0.0.1");
    m_userName = QString("root");
    m_connectCount = 0;
    m_sessionComment = QString("");
    m_driver = QString();
    m_netType = 0;
    m_password = QString("");
    m_port = 3306;
    m_loginPromt = false;
    m_windowsAuth = false;
    m_sshHost = QString("");
    m_sshPort = 22;
    m_sshUser = QString("");
    m_sshPassword = QString("");
    m_sshTimeout = 60;
    m_sshPrivateKey = QString("");
    m_sshLocalPort = 23;
    m_sshPlinkExe = QString("");
    m_sshPlinkTimeout = 60;
    m_wantSsl = false;
    m_sslPrivateKey = QString("");
    m_sslCaCertificate = QString("");
    m_sslCertificate = QString("");
    m_sslCipher = QString("");
    m_compressed = false;
    m_refusedCount = 0;
    m_sessionCreated = QDateTime::currentDateTime();
    m_startupScriptFilename = QString("");
    m_lastUsedDB = QString("");
    m_queryTimeOut = 60;
    m_pingTimeOut = 60;
    m_clientTimeZone = false;
    m_fullTableStatus = false;
}

SessionTreeItem::SessionTreeItem(const QJsonObject &sessionObject, TreeItem *parentItem)
    : TreeItem(parentItem)
{
    m_sessionName = sessionObject.value("sessionName").toString();
    m_hostName = sessionObject.value("hostName").toString();
    m_serverVersion = sessionObject.value("serverVersion").toString();
    m_userName = sessionObject.value("userName").toString();
    m_lastConnect = sessionObject.value("lastConnect").toVariant().toDateTime();
    m_connectCount = sessionObject.value("connectCount").toInt();
    m_sessionComment = sessionObject.value("sessionComment").toString();
    m_driver = sessionObject.value("driver").toString();
    m_netType = sessionObject.value("netType").toInt();
    m_password = sessionObject.value("password").toString();
    m_port = sessionObject.value("port").toInt();
    m_loginPromt = sessionObject.value("loginPromt").toBool();
    m_windowsAuth = sessionObject.value("windowsAuth").toBool();
    m_allDatabaseStr = sessionObject.value("allDatabaseStr").toString();
    m_sshHost = sessionObject.value("sshHost").toString();
    m_sshPort = sessionObject.value("sshPort").toInt();
    m_sshUser = sessionObject.value("sshUser").toString();
    m_sshPassword = sessionObject.value("sshPassword").toString();
    m_sshTimeout = sessionObject.value("sshTimeout").toInt();
    m_sshPrivateKey = sessionObject.value("sshPrivateKey").toString();
    m_sshLocalPort = sessionObject.value("sshLocalPort").toInt();
    m_sshPlinkExe = sessionObject.value("sshPlinkExe").toString();
    m_sshPlinkTimeout = sessionObject.value("sshPlinkTimeout").toInt();
    m_wantSsl = sessionObject.value("wantSsl").toBool();
    m_sslPrivateKey = sessionObject.value("sslPrivateKey").toString();
    m_sslCaCertificate = sessionObject.value("sslCaCertificate").toString();
    m_sslCertificate = sessionObject.value("sslCertificate").toString();
    m_sslCipher = sessionObject.value("sslCipher").toString();
    m_compressed = sessionObject.value("compressed").toBool();
    m_refusedCount = sessionObject.value("refusedCount").toInt();
    m_sessionCreated = QDateTime::fromString(sessionObject.value("sessionCreated").toString(), Qt::ISODate);
    m_startupScriptFilename = sessionObject.value("startupScriptFilename").toString();
    m_lastUsedDB = sessionObject.value("lastUsedDB").toString();
    m_queryTimeOut = sessionObject.value("queryTimeOut").toInt();
    m_pingTimeOut = sessionObject.value("pingTimeOut").toInt();
    m_clientTimeZone = sessionObject.value("clientTimeZone").toBool();
    m_fullTableStatus = sessionObject.value("fullTableStatus").toBool();
}

QVariant SessionTreeItem::data(int column) const
{
    switch(column) {
    case Field::SessionName:
        return m_sessionName;
    case Field::HostName:
        return m_hostName;
    case Field::ServerVersion:
        return m_serverVersion;
    case Field::UserName:
        return m_userName;
    case Field::LastConnect:
        return m_lastConnect;
    case Field::ConnectCount:
        return m_connectCount;
    case Field::SessionComment:
        return m_sessionComment;
    case Field::Driver:
        return m_driver;
    case Field::NetType:
        return m_netType;
    case Field::Password:
        return m_password;
    case Field::Port:
        return m_port;
    case Field::LoginPromt:
        return m_loginPromt;
    case Field::WindowsAuth:
        return m_windowsAuth;
    case Field::AllDatabaseStr:
        return m_allDatabaseStr;
    case Field::SshHost:
        return m_sshHost;
    case Field::SshPort:
        return m_sshPort;
    case Field::SshUser:
        return m_sshUser;
    case Field::SshPassword:
        return m_sshPassword;
    case Field::SshTimeout:
        return m_sshTimeout;
    case Field::SshPrivateKey:
        return m_sshPrivateKey;
    case Field::SshLocalPort:
        return m_sshLocalPort;
    case Field::SshPlinkExe:
        return m_sshPlinkExe;
    case Field::SshPlinkTimeout:
        return m_sshPlinkTimeout;
    case Field::WantSsl:
        return m_wantSsl;
    case Field::SslPrivateKey:
        return m_sslPrivateKey;
    case Field::SslCaCertificate:
        return m_sslCaCertificate;
    case Field::SslCertificate:
        return m_sslCertificate;
    case Field::SslCipher:
        return m_sslCipher;
    case Field::Compressed:
        return m_compressed;
    case Field::RefusedCount:
        return m_refusedCount;
    case Field::SessionCreated:
        return m_sessionCreated;
    case Field::StartupScriptFilename:
        return m_startupScriptFilename;
    case Field::LastUsedDB:
        return m_lastUsedDB;
    case Field::QueryTimeOut:
        return m_queryTimeOut;
    case Field::PingTimeOut:
        return m_pingTimeOut;
    case Field::ClientTimeZone:
        return m_clientTimeZone;
    case Field::FullTableStatus:
        return m_fullTableStatus;
    }

    return QVariant();
}

bool SessionTreeItem::setData(int column, const QVariant &data)
{
    switch (column) {
    case Field::SessionName:
        m_sessionName = data.toString();
        break;
    case Field::HostName:
        m_hostName = data.toString();
        break;
    case Field::ServerVersion:
        m_serverVersion = data.toString();
        break;
    case Field::UserName:
        m_userName = data.toString();
        break;
    case Field::LastConnect:
        m_lastConnect = data.toDateTime();
        break;
    case Field::ConnectCount:
        m_connectCount = data.toInt();
        break;
    case Field::SessionComment:
        m_sessionComment = data.toString();
        break;
    case Field::Driver:
        m_driver = data.toString();
        break;
    case Field::NetType:
        m_netType = data.toInt();
        break;
    case Field::Password:
        m_password = data.toString();
        break;
    case Field::Port:
        m_port = data.toInt();
        break;
    case Field::LoginPromt:
        m_loginPromt = data.toBool();
        break;
    case Field::WindowsAuth:
        m_windowsAuth = data.toBool();
        break;
    case Field::AllDatabaseStr:
        m_allDatabaseStr = data.toString();
        break;
    case Field::SshHost:
        m_sshHost = data.toString();
        break;
    case Field::SshPort:
        m_sshPort = data.toInt();
        break;
    case Field::SshUser:
        m_sshUser = data.toString();
        break;
    case Field::SshPassword:
        m_sshPassword = data.toString();
        break;
    case Field::SshTimeout:
        m_sshTimeout = data.toInt();
        break;
    case Field::SshPrivateKey:
        m_sshPrivateKey = data.toString();
        break;
    case Field::SshLocalPort:
        m_sshLocalPort = data.toInt();
        break;
    case Field::SshPlinkExe:
        m_sshPlinkExe = data.toString();
        break;
    case Field::SshPlinkTimeout:
        m_sshPlinkTimeout = data.toInt();
        break;
    case Field::WantSsl:
        m_wantSsl = data.toBool();
        break;
    case Field::SslPrivateKey:
        m_sslPrivateKey = data.toString();
        break;
    case Field::SslCaCertificate:
        m_sslCaCertificate = data.toString();
        break;
    case Field::SslCertificate:
        m_sslCertificate = data.toString();
        break;
    case Field::SslCipher:
        m_sslCipher = data.toString();
        break;
    case Field::Compressed:
        m_compressed = data.toBool();
        break;
    case Field::RefusedCount:
        m_refusedCount = data.toInt();
        break;
    case Field::SessionCreated:
        m_sessionCreated = data.toDateTime();
        break;
    case Field::StartupScriptFilename:
        m_startupScriptFilename = data.toString();
        break;
    case Field::LastUsedDB:
        m_lastUsedDB = data.toString();
        break;
    case Field::QueryTimeOut:
        m_queryTimeOut = data.toInt();
        break;
    case Field::PingTimeOut:
        m_pingTimeOut = data.toInt();
        break;
    case Field::ClientTimeZone:
        m_clientTimeZone = data.toBool();
        break;
    case Field::FullTableStatus:
        m_fullTableStatus = data.toBool();
        break;
    default:
        return false;
    }

    return true;
}

QVariant SessionTreeItem::icon(int column) const
{
    if (column == Field::SessionName)
        if (m_serverVersion.toLower().contains("percona")) {
            return QIcon(":/icons/server-percona.png");
        }
        else if (m_serverVersion.toLower().contains("mariadb")) {
            return QIcon(":/icons/server-mariadb.png");
        }
        else {
            return QIcon(":/icons/server-mysql.png");
        }


    return QVariant();
}

bool SessionTreeItem::canInsertChild() const
{
    return false;
}

bool SessionTreeItem::canEdit() const
{
    return true;
}

QJsonObject SessionTreeItem::toJson()
{
    QJsonObject session;

    session.insert("type", QJsonValue::fromVariant(1));

    session.insert("sessionName", QJsonValue::fromVariant(m_sessionName));
    session.insert("hostName", QJsonValue::fromVariant(m_hostName));
    session.insert("serverVersion", QJsonValue::fromVariant(m_serverVersion));
    session.insert("userName", QJsonValue::fromVariant(m_userName));
    session.insert("lastConnect", QJsonValue::fromVariant(m_lastConnect));
    session.insert("connectCount", QJsonValue::fromVariant(m_connectCount));
    session.insert("sessionComment", QJsonValue::fromVariant(m_sessionComment));
    session.insert("driver", QJsonValue::fromVariant(m_driver));
    session.insert("netType", QJsonValue::fromVariant(m_netType));
    session.insert("password", QJsonValue::fromVariant(m_password));
    session.insert("port", QJsonValue::fromVariant(m_port));
    session.insert("loginPromt", QJsonValue::fromVariant(m_loginPromt));
    session.insert("windowsAuth", QJsonValue::fromVariant(m_windowsAuth));
    session.insert("allDatabaseStr", QJsonValue::fromVariant(m_allDatabaseStr));
    session.insert("sshHost", QJsonValue::fromVariant(m_sshHost));
    session.insert("sshPort", QJsonValue::fromVariant(m_sshPort));
    session.insert("sshUser", QJsonValue::fromVariant(m_sshUser));
    session.insert("sshPassword", QJsonValue::fromVariant(m_sshPassword));
    session.insert("sshTimeout", QJsonValue::fromVariant(m_sshTimeout));
    session.insert("sshPrivateKey", QJsonValue::fromVariant(m_sshPrivateKey));
    session.insert("sshLocalPort", QJsonValue::fromVariant(m_sshLocalPort));
    session.insert("sshPlinkExe", QJsonValue::fromVariant(m_sshPlinkExe));
    session.insert("sshPlinkTimeout", QJsonValue::fromVariant(m_sshPlinkTimeout));
    session.insert("wantSsl", QJsonValue::fromVariant(m_wantSsl));
    session.insert("sslPrivateKey", QJsonValue::fromVariant(m_sslPrivateKey));
    session.insert("sslCaCertificate", QJsonValue::fromVariant(m_sslCaCertificate));
    session.insert("sslCertificate", QJsonValue::fromVariant(m_sslCertificate));
    session.insert("sslCipher", QJsonValue::fromVariant(m_sslCipher));
    session.insert("compressed", QJsonValue::fromVariant(m_compressed));
    session.insert("refusedCount", QJsonValue::fromVariant(m_refusedCount));
    session.insert("sessionCreated", QJsonValue::fromVariant(m_sessionCreated.toString(Qt::ISODate)));
    session.insert("startupScriptFilename", QJsonValue::fromVariant(m_startupScriptFilename));
    session.insert("lastUsedDB", QJsonValue::fromVariant(m_lastUsedDB));
    session.insert("queryTimeOut", QJsonValue::fromVariant(m_queryTimeOut));
    session.insert("pingTimeOut", QJsonValue::fromVariant(m_pingTimeOut));
    session.insert("clientTimeZone", QJsonValue::fromVariant(m_clientTimeZone));
    session.insert("fullTableStatus", QJsonValue::fromVariant(m_fullTableStatus));



    return session;
}

QString SessionTreeItem::name() const
{
    return m_sessionName;
}

QString SessionTreeItem::hostName() const
{
    return m_hostName;
}

unsigned int SessionTreeItem::port() const
{
    return m_port;
}

QString SessionTreeItem::userName() const
{
    return m_userName;
}

QString SessionTreeItem::password() const
{
    return m_password;
}

bool SessionTreeItem::loginPrompt() const
{
    return m_loginPromt;
}

QString SessionTreeItem::driver() const
{
    return m_driver;
}

void SessionTreeItem::updateLastConnect()
{
    m_lastConnect = QDateTime::currentDateTime();
}

void SessionTreeItem::incrementConnectsCount()
{
    m_connectCount++;
}

void SessionTreeItem::incrementRefusedCount()
{
    m_refusedCount++;
}
