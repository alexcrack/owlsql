#ifndef FOLDERTREEITEM_H
#define FOLDERTREEITEM_H

#include "treeitem.h"

class FolderTreeItem : public TreeItem
{
public:
    enum Field {
        FolderName = 0
    };

    explicit FolderTreeItem(const QString &name, TreeItem *parentItem);

    QVariant data(int column) const;
    bool setData(int column, const QVariant &data) override;
    QVariant icon(int column) const;

    bool canInsertChild() const;
    QJsonObject toJson() override;

private:
    QString m_folderName;
};

#endif // FOLDERTREEITEM_H
