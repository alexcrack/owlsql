#ifndef ISDIRTYDATAWIDGETMAPPER_H
#define ISDIRTYDATAWIDGETMAPPER_H

#include <QDataWidgetMapper>
#include <QWidget>
#include <QModelIndex>
#include <QDebug>
#include <QSignalMapper>

class IsDirtyDataWidgetMapper : public QDataWidgetMapper
{
    Q_OBJECT

public:
    explicit IsDirtyDataWidgetMapper(QObject *parent = 0);
    ~IsDirtyDataWidgetMapper();

    /** Use this method each time the model gets correctly submitted */
    void onModelSubmitted();

    /** Return true if the current values of the mapped widget differs from the original model values */
    bool isDirty() const;

    /** Overload add mapping method for additional signals */
    void addMapping(QWidget *widget, int section);
    void addMapping(QWidget *widget, int section, const QByteArray &propertyName);


public Q_SLOTS:
    /** Overload method (creates the internal cache) */
    void setCurrentIndex(int index);

signals:
    void dirtyStateChanged(bool isDirtied);

private:
    void refreshCache();
    void setChangesListener(QWidget *widget);

    QHash<QWidget*, QVariant> _original;
    bool sendSignalChanged = false;

private slots:
    void s_widgetChanged();
    void s_widgetIndexChanged(int index);
    void s_widgetTextChanged(QString text);
    void s_widgetCheckChanged(bool state);
};

#endif // ISDIRTYDATAWIDGETMAPPER_H
