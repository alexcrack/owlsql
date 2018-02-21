#ifndef SESSIONTREEITEM_H
#define SESSIONTREEITEM_H

#include "treeitem.h"
#include <QJsonObject>

class SessionTreeItem : public TreeItem
{
public:
    explicit SessionTreeItem(const QString &name, TreeItem *parentItem);
    explicit SessionTreeItem(const QJsonObject &sessionObject, TreeItem *parentItem);

    QVariant data(int column) const;
    QVariant icon(int column) const;

    bool canInsertChild() const;

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
