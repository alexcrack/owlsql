#include "sessionstreemodel.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "foldertreeitem.h"
#include "sessiontreeitem.h"

SessionsTreeModel::SessionsTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    QList<QVariant> rootData;
    rootData << QString(tr("Session name")) << QString(tr("Host")) << QString(tr("Server version"))
             << QString(tr("Username")) << QString(tr("Last connect")) << QString(tr("Connects"))
             << QString(tr("Comment"));

    rootItem = new TreeItem(rootData);

    setupModelData();
}

SessionsTreeModel::~SessionsTreeModel()
{
    delete rootItem;
}

QVariant SessionsTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal)
        return rootItem->data(section);

    if (role == Qt::SizeHintRole) {
        return QSize(24, 24);
    }

    return QVariant();
}

bool SessionsTreeModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if (value != headerData(section, orientation, role)) {
        // FIXME: Implement me!
        emit headerDataChanged(orientation, section, section);
        return true;
    }
    return false;
}

QModelIndex SessionsTreeModel::index(int row, int column, const QModelIndex &parent) const
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

QModelIndex SessionsTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int SessionsTreeModel::rowCount(const QModelIndex &parent) const
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

int SessionsTreeModel::columnCount(const QModelIndex &parent) const
{
    return SessionTreeItem::FullTableStatus + 1;
}

QVariant SessionsTreeModel::data(const QModelIndex &index, int role) const
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

bool SessionsTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role == Qt::EditRole && data(index, role) != value) {
        qDebug() << "Set data " << value;

        TreeItem *item = getItem(index);

        bool result = item->setData(index.column(), value);

        if (result)
            emit dataChanged(index, index, QVector<int>() << role);

        return true;
    }

    return false;
}

Qt::ItemFlags SessionsTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (index.column() == 0)
        return flags |= Qt::ItemIsEditable;

    return flags;
}

bool SessionsTreeModel::insertRows(int row, int count, const QModelIndex &parent)
{
    beginInsertRows(parent, row, row + count - 1);
    // FIXME: Implement me!
    endInsertRows();
}

bool SessionsTreeModel::insertColumns(int column, int count, const QModelIndex &parent)
{
    beginInsertColumns(parent, column, column + count - 1);
    // FIXME: Implement me!
    endInsertColumns();
}

bool SessionsTreeModel::removeRows(int row, int count, const QModelIndex &parent)
{
    beginRemoveRows(parent, row, row + count - 1);
    // FIXME: Implement me!
    endRemoveRows();

    return true;
}

bool SessionsTreeModel::removeColumns(int column, int count, const QModelIndex &parent)
{
    beginRemoveColumns(parent, column, column + count - 1);
    // FIXME: Implement me!
    endRemoveColumns();

    return true;
}

bool SessionsTreeModel::submit()
{
    return true;
}

bool SessionsTreeModel::saveModelData()
{
    QFile sessionsFile;

    sessionsFile.setFileName("sessions.json");
    sessionsFile.open(QIODevice::WriteOnly | QIODevice::Text);

    QJsonObject object;
    QJsonArray sessions;

    foreach (auto session, rootItem->children()) {
        sessions.push_back(session->toJson());
    }

    object.insert("sessions", sessions);

    QJsonDocument document(object);

    sessionsFile.write(document.toJson());

    sessionsFile.close();

    QSettings settings;

    settings.beginGroup("Sessions");

    settings.setValue("SessionsTree", document.toJson(QJsonDocument::Compact));

    settings.endGroup();
}

void SessionsTreeModel::setupModelData()
{
    QFile sessionsFile;
    sessionsFile.setFileName("sessions.json");
    sessionsFile.open(QIODevice::ReadOnly | QIODevice::Text);

    QString jsonData = sessionsFile.readAll();
    sessionsFile.close();

    QJsonDocument sessionsDocument = QJsonDocument::fromJson(jsonData.toUtf8());
    QJsonObject sessionsObject = sessionsDocument.object();

    QJsonArray sessions = sessionsObject.value("sessions").toArray();

    addSessions(sessions, rootItem);
}

void SessionsTreeModel::addSessions(const QJsonArray &sessions, TreeItem *parent)
{
    foreach(const QJsonValue &session, sessions) {
        QJsonObject sessionObject = session.toObject();

        int type = sessionObject.value("type").toInt();
        QString sessionName = sessionObject.value("name").toString();

        if (type == 1) {
            SessionTreeItem *sessionItem = new SessionTreeItem(sessionObject, parent);

            parent->appendChild(sessionItem);

        } else if (type == 2) {
            FolderTreeItem *folderItem = new FolderTreeItem(sessionName, parent);

            parent->appendChild(folderItem);

            if (sessionObject.contains("sessions")) {
                addSessions(sessionObject.value("sessions").toArray(), folderItem);
            }
        }
    }
}

TreeItem* SessionsTreeModel::getItem(const QModelIndex &index) const
{
    if (index.isValid()) {
        TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
        if (item)
            return item;
    }

    return rootItem;
}

void SessionsTreeModel::setDirty(const QModelIndex &index, bool isDirty)
{
    TreeItem *item = getItem(index);

    item->setDirty(isDirty);

    emit dataChanged(index, index);
}

QModelIndex SessionsTreeModel::findParentToInsert(const QModelIndex &index) const
{
    QModelIndex parentIndex = index;

    while (!getItem(parentIndex)->canInsertChild())
        parentIndex = parentIndex.parent();

    return parentIndex;
}

QModelIndex SessionsTreeModel::createItem(const TreeItem *item, const QModelIndex &index)
{

}

QModelIndex SessionsTreeModel::createFolder(const QString &name, const QModelIndex &index)
{
    QModelIndex parent = findParentToInsert(index);

    TreeItem *item = getItem(parent);

    bool success;
    int row = index.row();
    if (parent == index)
        row = item->childCount();

    beginInsertRows(parent, row, row);

    success = item->insertChild(row, new FolderTreeItem(name, item));

    endInsertRows();

    if (success)
        return this->index(row, 0, parent);

    return QModelIndex();
}

QModelIndex SessionsTreeModel::createSession(const QString &name, const QModelIndex &index)
{
    QModelIndex parent = findParentToInsert(index);

    TreeItem *item = getItem(parent);

    bool success;
    int row = index.row();
    if (parent == index)
        row = item->childCount();

    beginInsertRows(parent, row, row);

    success = item->insertChild(row, new SessionTreeItem(name, item));

    endInsertRows();

    if (success)
        return this->index(row, 0, parent);

    return QModelIndex();
}

bool SessionsTreeModel::deleteSession(const QModelIndex &index)
{
    QModelIndex parent = index.parent();
    TreeItem *item = getItem(index);
    TreeItem *parentItem = getItem(parent);
    int row = parentItem->children().indexOf(item);

    beginRemoveRows(parent, row, row);

    bool result = parentItem->removeChild(row);

    endRemoveRows();

    return result;
}
