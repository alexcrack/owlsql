#ifndef SESSIONDELEGATE_H
#define SESSIONDELEGATE_H

#include <QItemDelegate>

class SessionDelegate : public QItemDelegate
{
    Q_OBJECT

public:
    SessionDelegate();
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
};

#endif // SESSIONDELEGATE_H
