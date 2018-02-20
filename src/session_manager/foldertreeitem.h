#ifndef FOLDERTREEITEM_H
#define FOLDERTREEITEM_H

#include "treeitem.h"

class FolderTreeItem : public TreeItem
{
public:
    explicit FolderTreeItem(const QString &name, TreeItem *parentItem);

    QVariant data(int column) const;
    QVariant icon(int column) const;

private:
    QString m_folderName;
};

#endif // FOLDERTREEITEM_H
