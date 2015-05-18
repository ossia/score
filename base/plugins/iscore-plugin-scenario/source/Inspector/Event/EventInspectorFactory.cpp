#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

#include <Document/Event/EventModel.hpp>

//using namespace iscore;

InspectorWidgetBase* EventInspectorFactory::makeWidget(
        const QObject* sourceElement,
        QWidget* parent)
{
    auto event = static_cast<const EventModel*>(sourceElement);
    return new EventInspectorWidget{event, parent};
}
