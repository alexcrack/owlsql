#ifndef TREEITEM_H
#define TREEITEM_H

#include <QList>
#include <QVariant>
#include <QIcon>

class TreeItem
{
public:
    explicit TreeItem(const QList<QVariant> &data, TreeItem *parentItem = 0);
    explicit TreeItem(TreeItem *parentItem = 0);
    ~TreeItem();

    void appendChild(TreeItem *child);

    virtual TreeItem *child(int row);
    virtual int childCount() const;
    int columnCount() const;
    virtual QVariant data(int column) const;
    virtual bool setData(int column, const QVariant &data);
    virtual int row() const;
    virtual TreeItem *parentItem();
    virtual QVariant icon(int column) const;

    virtual bool canInsertChild() const;
    bool insertChild(int position, TreeItem *item);

private:
    TreeItem *m_parentItem;
    QList<TreeItem *> m_childItems;
    QList<QVariant> m_itemData;
};

#endif // TREEITEM_H
