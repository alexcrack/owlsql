#include "foldertreeitem.h"

FolderTreeItem::FolderTreeItem(const QString &name, TreeItem *parentItem)
    : TreeItem(parentItem), m_folderName(name)
{

}

QVariant FolderTreeItem::data(int column) const
{
    if (column == Field::FolderName)
        return m_folderName;

    return QVariant();
}

bool FolderTreeItem::setData(int column, const QVariant &data)
{
    switch (column) {
    case Field::FolderName:
        m_folderName = data.toString();
        return true;
    default:
        return false;
    }
}

QVariant FolderTreeItem::icon(int column) const
{
    if (column == 0)
        return QIcon(":/icons/folder.svg");
}



bool FolderTreeItem::canInsertChild() const
{
    return true;
}
