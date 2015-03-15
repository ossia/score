#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

#include <Document/Event/EventModel.hpp>

//using namespace iscore;

InspectorWidgetBase* EventInspectorFactory::makeWidget(QObject* sourceElement, QWidget* parent)
{
    auto event = static_cast<EventModel*>(sourceElement);
    return new EventInspectorWidget{event, parent};
}
