#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

#include <Document/Event/EventModel.hpp>

//using namespace iscore;

InspectorWidgetBase* EventInspectorFactory::makeWidget(QObject* sourceElement)
{
    auto event = static_cast<EventModel*>(sourceElement);
    return new EventInspectorWidget(event);

}

InspectorWidgetBase* EventInspectorFactory::makeWidget(QList<QObject*> sourceElements)
{
    // @todo make a tabbed view when there is a list.
    return new EventInspectorWidget(static_cast<EventModel*>(sourceElements.at(0)));
}
