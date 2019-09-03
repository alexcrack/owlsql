#ifndef DBCONNECTION_H
#define DBCONNECTION_H

#include "sessiontreeitem.h"
#include "credentialsinputdialog.h"
#include <QObject>
#include <QUuid>
#include <QtSql>

class DbConnection : public QObject
{
    Q_OBJECT
public:
    explicit DbConnection(SessionTreeItem *item);
    ~DbConnection();

    SessionTreeItem* session();
    bool connect();
    bool disconnect();

    QUuid connectionUuid;
    QSqlDatabase db;

Q_SIGNALS:
    void log(QString message);

private:
    SessionTreeItem *m_item;
};

#endif // DBCONNECTION_H
