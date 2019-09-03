#include "servertreeitem.h"

ServerTreeItem::ServerTreeItem(TreeItem *parentItem)
    : TreeItem(parentItem)
{

}

ServerTreeItem::ServerTreeItem(DbConnection *connection, TreeItem *parentItem)
    : TreeItem(parentItem)
{
    m_connection = connection;
}

QVariant ServerTreeItem::data(int column) const
{
    switch (column) {
    case 0:
        return m_connection->session()->name();
    case 1:
        return QVariant();
    default:
        return QVariant();
    }
}

QVariant ServerTreeItem::icon(int column) const
{
    if (column == 0)
        return m_connection->session()->icon(column);
}
