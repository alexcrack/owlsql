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
    case 0:
        return m_sessionName;
    case 1:
        return m_hostName;
    case 2:
        return m_serverFullVersion;
    case 3:
        return m_user;
    case 6:
        return m_comment;
    }

    return QVariant();
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
