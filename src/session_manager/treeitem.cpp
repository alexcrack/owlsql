#include "treeitem.h"

TreeItem::TreeItem(const QList<QVariant> &data, TreeItem *parentItem)
{
    m_parentItem = parentItem;
    m_itemData = data;
}

TreeItem::TreeItem(TreeItem *parentItem)
{
    m_parentItem = parentItem;
}

TreeItem::~TreeItem()
{
    qDeleteAll(m_childItems);
}

void TreeItem::appendChild(TreeItem *child)
{
    m_childItems.append(child);
}

TreeItem *TreeItem::child(int row)
{
    return m_childItems.value(row);
}

int TreeItem::childCount() const
{
    return m_childItems.count();
}

int TreeItem::row() const
{
    if (m_parentItem)
        return m_parentItem->m_childItems.indexOf(const_cast<TreeItem*>(this));

    return 0;
}

int TreeItem::columnCount() const
{
    return m_itemData.count();
}

QVariant TreeItem::data(int column) const
{
    return m_itemData.value(column);
}

TreeItem *TreeItem::parentItem()
{
    return m_parentItem;
}

QVariant TreeItem::icon(int column) const
{
    return QVariant();
}

bool TreeItem::canInsertChild() const
{
    return true;
}

bool TreeItem::insertChild(int position, TreeItem *item)
{
    if (position < 0 || position > m_childItems.count())
        return false;

    m_childItems.insert(position, item);

    return true;
}
