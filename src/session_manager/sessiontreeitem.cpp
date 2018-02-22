#include "sessiontreeitem.h"

SessionTreeItem::SessionTreeItem(const QString &name, TreeItem *parentItem)
    : TreeItem(parentItem), m_sessionName(name)
{

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
        return true;
    default:
        return false;
    }
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
