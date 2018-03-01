#ifndef SERVERTREEITEM_H
#define SERVERTREEITEM_H

#include "session_manager/treeitem.h"
#include "session_manager/sessiontreeitem.h"

class ServerTreeItem : public TreeItem
{
public:
    explicit ServerTreeItem(TreeItem *pa);
    explicit ServerTreeItem(QString &sessionName, SessionTreeItem *sessionItem, TreeItem *parentItem);

    QVariant data(int column) const;
    QVariant icon(int column) const;

private:
    SessionTreeItem *m_sessionItem;
    QString m_sessionName;
};

#endif // SERVERTREEITEM_H
