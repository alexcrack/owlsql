#ifndef SESSIONTREEITEM_H
#define SESSIONTREEITEM_H

#include "treeitem.h"

class SessionTreeItem : public TreeItem
{
public:
    explicit SessionTreeItem(const QString &name, TreeItem *parentItem);

    QVariant data(int column) const;
    QVariant icon(int column) const;

private:
    QString m_sessionName;
    QString m_hostName;
    int m_port;
    QString m_user;
    QString m_password;
};

#endif // SESSIONTREEITEM_H
