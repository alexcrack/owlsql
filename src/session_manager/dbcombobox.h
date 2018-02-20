#ifndef DBCOMBOBOX_H
#define DBCOMBOBOX_H

#include <QComboBox>
#include <QWidget>


class DBComboBox : public QComboBox
{
    Q_OBJECT

public:
    explicit DBComboBox(QWidget *parent = 0);

    virtual void showPopup();
};

#endif // DBCOMBOBOX_H
