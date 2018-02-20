#include "dbcombobox.h"
#include <QDebug>

DBComboBox::DBComboBox(QWidget* parent)
{

}

void DBComboBox::showPopup()
{
    while (count() > 0) { removeItem(0); }
    addItem("");
    addItem("hello");
    addItem("World");

    QComboBox::showPopup();
}
