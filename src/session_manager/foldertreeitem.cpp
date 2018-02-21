#include "foldertreeitem.h"

FolderTreeItem::FolderTreeItem(const QString &name, TreeItem *parentItem)
    : TreeItem(parentItem), m_folderName(name)
{

}

QVariant FolderTreeItem::data(int column) const
{
    if (column == 0)
        return m_folderName;

    return QVariant();
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
