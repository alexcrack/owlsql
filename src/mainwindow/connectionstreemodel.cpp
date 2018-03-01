#include "connectionstreemodel.h"

ConnectionsTreeModel::ConnectionsTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{

    QList<QVariant> rootData;
    rootData << QString(tr("Database")) << QString(tr("Size"));

    rootItem = new TreeItem(rootData);
}

QVariant ConnectionsTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        return rootItem->data(section);

    if (role == Qt::SizeHintRole) {
        return QSize(24, 24);
    }

    return QVariant();
}

bool ConnectionsTreeModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if (value != headerData(section, orientation, role)) {
        // FIXME: Implement me!
        emit headerDataChanged(orientation, section, section);
        return true;
    }
    return false;
}

QModelIndex ConnectionsTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);

    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex ConnectionsTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int ConnectionsTreeModel::rowCount(const QModelIndex &parent) const
{
    TreeItem *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem*>(parent.internalPointer());

    return parentItem->childCount();
}

int ConnectionsTreeModel::columnCount(const QModelIndex &parent) const
{
    return 2;
}

//bool ConnectionsTreeModel::hasChildren(const QModelIndex &parent) const
//{
//    // FIXME: Implement me!
//}

//bool ConnectionsTreeModel::canFetchMore(const QModelIndex &parent) const
//{
//    // FIXME: Implement me!
//    return false;
//}

//void ConnectionsTreeModel::fetchMore(const QModelIndex &parent)
//{
//    // FIXME: Implement me!
//}

QVariant ConnectionsTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    if (role == Qt::DisplayRole) {
        return item->data(index.column());
    }

    if (role == Qt::DecorationRole) {
        return item->icon(index.column());
    }

    if (role == Qt::SizeHintRole) {
        return QSize(24, 24);
    }

    if (role == Qt::EditRole) {
        return item->data(index.column());
    }

    return QVariant();
}

bool ConnectionsTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (data(index, role) != value) {
        // FIXME: Implement me!
        emit dataChanged(index, index, QVector<int>() << role);
        return true;
    }
    return false;
}

Qt::ItemFlags ConnectionsTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (index.column() == 0)
        return flags |= Qt::ItemIsEditable;

    return flags;
}

bool ConnectionsTreeModel::insertRows(int row, int count, const QModelIndex &parent)
{
    beginInsertRows(parent, row, row + count - 1);
    // FIXME: Implement me!
    endInsertRows();
}

bool ConnectionsTreeModel::insertColumns(int column, int count, const QModelIndex &parent)
{
    beginInsertColumns(parent, column, column + count - 1);
    // FIXME: Implement me!
    endInsertColumns();
}

bool ConnectionsTreeModel::removeRows(int row, int count, const QModelIndex &parent)
{
    beginRemoveRows(parent, row, row + count - 1);
    // FIXME: Implement me!
    endRemoveRows();
}

bool ConnectionsTreeModel::removeColumns(int column, int count, const QModelIndex &parent)
{
    beginRemoveColumns(parent, column, column + count - 1);
    // FIXME: Implement me!
    endRemoveColumns();
}

TreeItem* ConnectionsTreeModel::getItem(const QModelIndex &index) const
{
    if (index.isValid()) {
        TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
        if (item)
            return item;
    }

    return rootItem;
}

QModelIndex ConnectionsTreeModel::addConnection(QString &sessionName, SessionTreeItem *sessionItem, const QModelIndex &parent)
{
    beginInsertRows(QModelIndex(), 0, 0);

    rootItem->appendChild(new ServerTreeItem(sessionName, sessionItem, rootItem));

    endInsertRows();

    return index(0, 0, QModelIndex());
}
