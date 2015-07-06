#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

#include <Document/Event/EventModel.hpp>

InspectorWidgetBase* EventInspectorFactory::makeWidget(
        const QObject* sourceElement,
        QWidget* parentWidget)
{
    return new EventInspectorWidget{static_cast<const EventModel*>(sourceElement),
                parentWidget};
}

QList<QString> EventInspectorFactory::correspondingObjectsNames() const
{
    return {"EventModel"};
}
