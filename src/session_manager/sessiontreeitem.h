#ifndef SESSIONTREEITEM_H
#define SESSIONTREEITEM_H

#include "treeitem.h"

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
        FullTableStatus
    };

    explicit SessionTreeItem(const QString &name, TreeItem *parentItem);
    explicit SessionTreeItem(const QJsonObject &sessionObject, TreeItem *parentItem);

    QVariant data(int column) const;
    bool setData(int column, const QVariant &data) override;
    QVariant icon(int column) const;

    bool canInsertChild() const;
    bool canEdit() const override;

    QJsonObject toJson() override;

private:
    QString m_sessionName;
    QString m_hostName;
    int m_port;
    QString m_user;
    QString m_password;
    QString m_serverFullVersion;
    QString m_comment;
};

#endif // SESSIONTREEITEM_H
