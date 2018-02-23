#include "sessiontreeitem.h"

SessionTreeItem::SessionTreeItem(const QString &name, TreeItem *parentItem)
    : TreeItem(parentItem), m_sessionName(name)
{
    m_hostName = QString("127.0.0.1");
    m_port = 3306;
    m_user = QString("root");
    m_password = QString("");
    m_comment = QString("");
}

SessionTreeItem::SessionTreeItem(const QJsonObject &sessionObject, TreeItem *parentItem)
    : TreeItem(parentItem)
{
    m_sessionName = sessionObject.value("name").toString();
    m_hostName = sessionObject.value("hostName").toString();
    m_port = sessionObject.value("port").toInt();
    m_user = sessionObject.value("user").toString();
    m_password = sessionObject.value("password").toString();
    m_serverFullVersion = sessionObject.value("serverFullVersion").toString();
    m_comment = sessionObject.value("comment").toString();
}

QVariant SessionTreeItem::data(int column) const
{
    switch(column) {
    case Field::SessionName:
        return m_sessionName;
    case Field::HostName:
        return m_hostName;
    case Field::ServerVersion:
        return m_serverFullVersion;
    case Field::UserName:
        return m_user;
    case Field::SessionComment:
        return m_comment;
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
    case Field::UserName:
        m_user = data.toString();
        break;
    case Field::SessionComment:
        m_comment = data.toString();
        break;
    default:
        return false;
    }

    return true;
}

QVariant SessionTreeItem::icon(int column) const
{
    if (column == 0)
        if (m_serverFullVersion.toLower().contains("percona")) {
            return QIcon(":/icons/server-percona.png");
        }
        else if (m_serverFullVersion.toLower().contains("mariadb")) {
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
    session.insert("name", QJsonValue::fromVariant(m_sessionName));
    session.insert("comment", QJsonValue::fromVariant(m_comment));
    session.insert("hostName", QJsonValue::fromVariant(m_hostName));
    session.insert("port", QJsonValue::fromVariant(m_port));
    session.insert("user", QJsonValue::fromVariant(m_user));
    session.insert("password", QJsonValue::fromVariant(m_password));
    session.insert("created", QJsonValue::fromVariant(QString("")));
    session.insert("connectionsCount", QJsonValue::fromVariant(0));
    session.insert("lastConnection", QJsonValue::fromVariant(QString("")));
    session.insert("refusedCount", QJsonValue::fromVariant(0));
    session.insert("serverFullVersion", QJsonValue::fromVariant(m_serverFullVersion));

    return session;
}
