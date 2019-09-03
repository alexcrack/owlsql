#include "isdirtydatawidgetmapper.h"
#include <QComboBox>

IsDirtyDataWidgetMapper::IsDirtyDataWidgetMapper(QObject *parent) : QDataWidgetMapper(parent) {}

IsDirtyDataWidgetMapper::~IsDirtyDataWidgetMapper() {}

void IsDirtyDataWidgetMapper::onModelSubmitted()
{
    refreshCache();
}

bool IsDirtyDataWidgetMapper::isDirty() const
{
    Q_ASSERT(orientation() == Qt::Horizontal);
    //Q_ASSERT(rootIndex() == QModelIndex());

    // cycle through all widgets the mapper supports
//    for(int i = 0; i < model()->columnCount(); i++) {
//        QWidget *mapWidget = mappedWidgetAt(i);
//        if (mapWidget) {
//            const QVariant &current = mapWidget->property(mappedPropertyName(mapWidget));
//            const QVariant &orig = _original.value(mapWidget);

//            // Special case of null original variant
//            if (orig.isNull() && current.toString().isEmpty())
//                continue;
//            if (current != orig) {//                qWarning() << "DIRTY row found:"
//                           << "orig" << orig
//                           << "current" << current;
//                return true;
//            }
//        }
//    }
//    return false;

    for(int i = 0; i < model()->columnCount(); i++) {
        QWidget *mapWidget = mappedWidgetAt(i);

        if (mapWidget && QString(mapWidget->metaObject()->className()) != "QLabel") {
            QByteArray p = mappedPropertyName(mapWidget);
            QModelIndex idx = model()->index(currentIndex(), i, rootIndex());
            if (idx.data(Qt::EditRole) != mapWidget->property(p)) {
                //qDebug() << "Widget" << mapWidget->metaObject()->className() << ":" << mapWidget->objectName() << p;
                qWarning() << "DIRTY found:" << "oring" << idx.data(Qt::EditRole) << "edited" << mapWidget->property(p);

                return true;
            }
        }
    }

    return false;
}

void IsDirtyDataWidgetMapper::setCurrentIndex(int index)
{
    sendSignalChanged = false;

    QDataWidgetMapper::setCurrentIndex(index);
    refreshCache();

    sendSignalChanged = true;
}

void IsDirtyDataWidgetMapper::refreshCache()
{
    //Q_ASSERT(orientation() == Qt::Horizontal);
    //Q_ASSERT(rootIndex() == QModelIndex());
    _original.clear();
    // cycle through all widgets the mapper supports
    for(int i = 0; i < model()->columnCount(); i++) {
        QWidget *mapWidget = mappedWidgetAt(i);
        if (mapWidget) {
            _original.insert(mapWidget, model()->data(model()->index(currentIndex(), i)));
        }
    }
}

void IsDirtyDataWidgetMapper::addMapping(QWidget *widget, int section)
{
    QDataWidgetMapper::addMapping(widget, section);
    setChangesListener(widget);
}

void IsDirtyDataWidgetMapper::addMapping(QWidget *widget, int section, const QByteArray &propertyName)
{
    QDataWidgetMapper::addMapping(widget, section, propertyName);
    setChangesListener(widget);
}

void IsDirtyDataWidgetMapper::setChangesListener(QWidget *widget)
{
    QString name(widget->metaObject()->className());

    if (name == "QComboBox") {
        connect(widget, SIGNAL(currentIndexChanged(int)), this, SLOT(s_widgetIndexChanged(int)));
    } else if (name == "DriverComboBox") {
        connect(widget, SIGNAL(currentIndexChanged(int)), this, SLOT(s_widgetIndexChanged(int)));
    } else if (name == "QLineEdit") {
        connect(widget, SIGNAL(textChanged(QString)), this, SLOT(s_widgetTextChanged(QString)));
    } else if (name == "QCheckBox") {
        connect(widget, SIGNAL(stateChanged(int)), this, SLOT(s_widgetIndexChanged(int)));
    } else if (name == "QSpinBox") {
        connect(widget, SIGNAL(valueChanged(int)), this, SLOT(s_widgetIndexChanged(int)));
    } else if (name == "QPlainTextEdit") {
        connect(widget, SIGNAL(textChanged()), this, SLOT(s_widgetChanged()));
    } else if (name == "QGroupBox") {
        connect(widget, SIGNAL(toggled(bool)), this, SLOT(s_widgetCheckChanged(bool)));
    }
}

void IsDirtyDataWidgetMapper::s_widgetChanged()
{
    if (sendSignalChanged)
        emit dirtyStateChanged(isDirty());
}

void IsDirtyDataWidgetMapper::s_widgetIndexChanged(int index)
{
    if (sendSignalChanged)
        emit dirtyStateChanged(isDirty());
}

void IsDirtyDataWidgetMapper::s_widgetTextChanged(QString text)
{
    //qWarning() << "Changed" << text;

    if (sendSignalChanged)
        emit dirtyStateChanged(isDirty());
}

void IsDirtyDataWidgetMapper::s_widgetCheckChanged(bool state)
{
    if (sendSignalChanged)
        emit dirtyStateChanged(isDirty());
}
