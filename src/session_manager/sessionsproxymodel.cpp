#include "sessionsproxymodel.h"

SessionsProxyModel::SessionsProxyModel()
    : QIdentityProxyModel()
{

}

QVariant SessionsProxyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

    if (role == Qt::DisplayRole && index.column() == 0 && item->isDirty()) {
        return item->data(index.column()).toString().append(" *");
    }

    if (role == Qt::FontRole && index.column() == 0 && item->isDirty()) {
        QFont font;
        font.setBold(true);

        return font;
    }

    return sourceModel()->data(index, role);
}
