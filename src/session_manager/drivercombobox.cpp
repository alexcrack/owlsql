#include "drivercombobox.h"

#include <QDebug>

DriverComboBox::DriverComboBox(QWidget *parent)
    : QComboBox(parent),
      m_drivers(QSqlDatabase::drivers())
{
    m_drivers.sort();

    foreach (auto driverName, m_drivers) {
        QString itemTitle = title(driverName);
        if (itemTitle.isNull()) {
            addItem(driverName, driverName);
        } else {
            addItem(QString("%1 (%2)").arg(itemTitle).arg(driverName), driverName);
        }
    }
}

QString DriverComboBox::currentDriver() const
{
    return currentData().toString();
}

void DriverComboBox::setCurrentDriver(QString driver)
{
    m_currentDriver = driver;
    setCurrentIndex(m_drivers.indexOf(driver));

    emit currentDriverChanged(driver);
}

QString DriverComboBox::title(QString driverName)
{
    if      (driverName == "QDB2")     return "IBM DB2";
    else if (driverName == "QIBASE")   return "Borland InterBase";
    else if (driverName == "QOCI")     return "Oracle Call Interface";
    else if (driverName == "QODBC")    return "ODBC";
    else if (driverName == "QODBC3")   return "ODBC";
    else if (driverName == "QTDS")     return "Sybase Adaptive Server";
    else if (driverName == "QMYSQL")   return "MySQL >=4.x";
    else if (driverName == "QMYSQL3")  return "MySQL 3.x";
    else if (driverName == "QPSQL")    return "PostgreSQL >=8.x";
    else if (driverName == "QPSQL7")   return "PostgreSQL 7.x";
    else if (driverName == "QSQLITE")  return "SQLite >=3.x";
    else if (driverName == "QSQLITE2") return "SQLite 2.x";
    else return QString();
}
