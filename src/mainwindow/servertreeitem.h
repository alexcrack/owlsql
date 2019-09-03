#ifndef SERVERTREEITEM_H
#define SERVERTREEITEM_H

#include "session_manager/treeitem.h"
#include "session_manager/dbconnection.h"

class ServerTreeItem : public TreeItem
{
public:
    explicit ServerTreeItem(TreeItem *pa);
    explicit ServerTreeItem(DbConnection *connection, TreeItem *parentItem);

    QVariant data(int column) const;
    QVariant icon(int column) const;


private:
    DbConnection *m_connection;
};

#endif // SERVERTREEITEM_H
