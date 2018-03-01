#ifndef SESSIONTREEITEM_H
#define SESSIONTREEITEM_H

#include "treeitem.h"
#include <QDateTime>

class SessionTreeItem : public TreeItem
{
public:
    enum Field {
        SessionName = 0,
        HostName,
        ServerVersion,
        UserName,
        LastConnect,
        ConnectCount,
        SessionComment,
        LISTED_BEFORE,
        NetType,
        Password,
        Port,
        LoginPromt,
        WindowsAuth,
        AllDatabaseStr,
        SshHost,
        SshPort,
        SshUser,
        SshPassword,
        SshTimeout,
        SshPrivateKey,
        SshLocalPort,
        SshPlinkExe,
        SshPlinkTimeout,
        WantSsl,
        SslPrivateKey,
        SslCaCertificate,
        SslCertificate,
        SslCipher,
        Compressed,
        RefusedCount,
        SessionCreated,
        StartupScriptFilename,
        LastUsedDB,
        QueryTimeOut,
        PingTimeOut,
        ClientTimeZone,
        FullTableStatus,
        NO_MORE_COLUMNS
    };

    explicit SessionTreeItem(const QString &name, TreeItem *parentItem);
    explicit SessionTreeItem(const QJsonObject &sessionObject, TreeItem *parentItem);

    QVariant data(int column) const;
    bool setData(int column, const QVariant &data) override;
    QVariant icon(int column) const;

    bool canInsertChild() const;
    bool canEdit() const override;

    QJsonObject toJson() override;
    QString name() const override;

    QString hostName() const;
    unsigned int port() const;
    QString userName() const;
    QString password() const;

private:
    QString m_sessionName;
    QString m_hostName;
    QString m_serverVersion;
    QString m_userName;
    QDateTime m_lastConnect;
    unsigned int m_connectCount;
    QString m_sessionComment;
    unsigned int m_netType;
    QString m_password;
    unsigned int m_port;
    bool m_loginPromt;
    bool m_windowsAuth;
    QString m_allDatabaseStr;
    QString m_sshHost;
    unsigned int m_sshPort;
    QString m_sshUser;
    QString m_sshPassword;
    unsigned int m_sshTimeout;
    QString m_sshPrivateKey;
    unsigned int m_sshLocalPort;
    QString m_sshPlinkExe;
    unsigned int m_sshPlinkTimeout;
    bool m_wantSsl;
    QString m_sslPrivateKey;
    QString m_sslCaCertificate;
    QString m_sslCertificate;
    QString m_sslCipher;
    bool m_compressed;
    unsigned int m_refusedCount;
    QDateTime m_sessionCreated;
    QString m_startupScriptFilename;
    QString m_lastUsedDB;
    unsigned int m_queryTimeOut;
    unsigned int m_pingTimeOut;
    QString m_clientTimeZone;
    QString m_fullTableStatus;
};

#endif // SESSIONTREEITEM_H
