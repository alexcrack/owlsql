#include "servertreeitem.h"

ServerTreeItem::ServerTreeItem(TreeItem *parentItem)
    : TreeItem(parentItem)
{

}

ServerTreeItem::ServerTreeItem(QString &sessionName, SessionTreeItem *sessionItem, TreeItem *parentItem)
    : TreeItem(parentItem)
{
    m_sessionName = sessionName;
    m_sessionItem = sessionItem;
}

QVariant ServerTreeItem::data(int column) const
{
    switch (column) {
    case 0:
        return m_sessionItem->name();
    case 1:
        return QVariant();
    default:
        return QVariant();
    }
}

QVariant ServerTreeItem::icon(int column) const
{
    if (column == 0)
        return m_sessionItem->icon(column);
}
