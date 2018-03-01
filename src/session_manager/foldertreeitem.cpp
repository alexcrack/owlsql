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

QJsonObject FolderTreeItem::toJson()
{
    QJsonObject folder;

    folder.insert("type", QJsonValue::fromVariant(2));
    folder.insert("name", QJsonValue::fromVariant(m_folderName));

    if (childCount() > 0) {
        QJsonArray sessions;

        foreach(auto child, children()) {
            sessions.push_back(child->toJson());
        }

        folder.insert("sessions", sessions);
    }

    return folder;
}

QString FolderTreeItem::name() const
{
    return m_folderName;
}
