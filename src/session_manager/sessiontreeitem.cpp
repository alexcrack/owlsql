#include "sessiontreeitem.h"

SessionTreeItem::SessionTreeItem(const QString &name, TreeItem *parentItem)
    : TreeItem(parentItem), m_sessionName(name)
{

}

QVariant SessionTreeItem::data(int column) const
{
    switch(column) {
    case 0:
        return m_sessionName;
    case 1:
        return m_hostName;
    }

    return QVariant();
}

QVariant SessionTreeItem::icon(int column) const
{
    if (column == 0)
        return QIcon(":/icons/server-percona.png");

    return QVariant();
}
