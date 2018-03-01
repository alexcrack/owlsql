#ifndef SESSIONSPROXYMODEL_H
#define SESSIONSPROXYMODEL_H

#include <QObject>
#include <QIdentityProxyModel>
#include <QFont>
#include "treeitem.h"


class SessionsProxyModel : public QIdentityProxyModel
{
public:
    SessionsProxyModel();

    QVariant data(const QModelIndex &index, int role) const override;
};

#endif // SESSIONSPROXYMODEL_H
