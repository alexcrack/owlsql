#include "dbconnection.h"

DbConnection::DbConnection(SessionTreeItem *item)
    : connectionUuid(QUuid::createUuid()),
      m_item(item)
{
    db = QSqlDatabase::addDatabase(m_item->m_driver, connectionUuid.toString());

    db.setHostName(m_item->m_hostName);
    db.setUserName(m_item->m_userName);
    db.setPassword(m_item->m_password);
    db.setPort(static_cast<int>(m_item->m_port));

    if (m_item->m_allDatabaseStr != "") {
        db.setDatabaseName(m_item->m_allDatabaseStr);
    }
}

DbConnection::~DbConnection()
{
    //delete m_item;
    QSqlDatabase::removeDatabase(connectionUuid.toString());
}

SessionTreeItem *DbConnection::session()
{
    return m_item;
}

bool DbConnection::connect()
{
    emit log(QString("/* Connecting to %1 with driver %2, username \"%3\", using password: %4... */")
             .arg(m_item->m_hostName)
             .arg(m_item->m_driver)
             .arg(m_item->m_userName)
             .arg(m_item->m_password.compare("") ? tr("No") : tr("Yes")));

    if (m_item->m_loginPromt) {
        CredentialsInputDialog credentialsInput;
        credentialsInput.forSession(m_item);

        if (credentialsInput.exec() == QDialog::Accepted) {
            db.setUserName(credentialsInput.userName);
            db.setPassword(credentialsInput.password);
        }
    }

    // TODO Set compressed protocol
    // TODO Connect to specified database

    if (db.open()) {
        QSqlQuery threadQuery(db);
        threadQuery.exec("SELECT CONNECTION_ID();");
        emit log(threadQuery.executedQuery());
        threadQuery.next();

        emit log(QString("/* Connected. Thread ID: %1 */").arg(threadQuery.value(0).toInt()));

        m_item->incrementConnectsCount();
        m_item->updateLastConnect();

        return true;
    } else {
        m_item->incrementRefusedCount();

        return false;
    };
}

bool DbConnection::disconnect()
{
    db.close();
    db.removeDatabase(connectionUuid.toString());

    return true;
}
