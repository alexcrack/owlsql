#include "sessiondelegate.h"
#include <QDateTime>
#include <QLabel>

SessionDelegate::SessionDelegate()
{

}

void SessionDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    if (editor->metaObject()->className() == QString("QLabel")) {
        QLabel *label = qobject_cast<QLabel*>(editor);
        QVariant data = index.data(Qt::DisplayRole);

        if (data.type() == QVariant::DateTime) {
            QDateTime dt = data.toDateTime();

            if (dt.isValid()) {
                label->setText(dt.toString(QString("yyyy-MM-dd hh:mm:ss")));
            } else {
                label->setText(QString(tr("Unknown or never")));
            }
        }

    } else {
        QItemDelegate::setEditorData(editor, index);
    }
}

void SessionDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    if (editor->metaObject()->className() != QString("QLabel")) {
        QItemDelegate::setModelData(editor, model, index);
    }
}
