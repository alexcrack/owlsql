#ifndef DRIVERCOMBOBOX_H
#define DRIVERCOMBOBOX_H

#include <QWidget>
#include <QComboBox>
#include <QtSql/QSqlDatabase>

class DriverComboBox : public QComboBox
{
    Q_OBJECT
    Q_PROPERTY(QString currentDriver READ currentDriver WRITE setCurrentDriver NOTIFY currentDriverChanged)
public:
    DriverComboBox(QWidget *parent = nullptr);
    QString currentDriver() const;
    void setCurrentDriver(QString driver);

signals:
    void currentDriverChanged(QString);

private:
    QString title(QString driverName);

    QStringList m_drivers;
    QString m_currentDriver;
};

#endif // DRIVERCOMBOBOX_H
