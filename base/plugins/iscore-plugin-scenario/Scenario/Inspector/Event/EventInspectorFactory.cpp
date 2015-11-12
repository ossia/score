#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

#include <Scenario/Document/Event/EventModel.hpp>

InspectorWidgetBase* EventInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parentWidget)
{
    return new EventInspectorWidget{
        static_cast<const EventModel&>(sourceElement),
                doc,
                parentWidget};
}

QList<QString> EventInspectorFactory::key_impl() const
{
    return {"EventModel"};
}
