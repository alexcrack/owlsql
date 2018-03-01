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

QList<TreeItem*> TreeItem::children()
{
    return m_childItems;
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

bool TreeItem::setData(int column, const QVariant &data)
{
    return true;
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

bool TreeItem::removeChild(int position)
{
    if (position < 0 || position > m_childItems.count())
        return false;

    m_childItems.removeAt(position);

    return true;
}

void TreeItem::setDirty(bool isDirty)
{
    m_isDirty = isDirty;
}

bool TreeItem::isDirty()
{
    return m_isDirty;
}

bool TreeItem::canEdit() const
{
    return false;
}

QJsonObject TreeItem::toJson()
{
    QJsonObject object;

    return object;
}

QString TreeItem::name() const
{
    return QString();
}
