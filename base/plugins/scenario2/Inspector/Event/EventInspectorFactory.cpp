#include "EventInspectorFactory.hpp"
#include "EventInspectorWidget.hpp"

#include <Document/Event/EventModel.hpp>
#include <QDebug>

//using namespace iscore;

InspectorWidgetBase* EventInspectorFactory::makeWidget (QObject* sourceElement)
{
	auto event = static_cast<EventModel*>(sourceElement);
	return new EventInspectorWidget(event);

}

InspectorWidgetBase* EventInspectorFactory::makeWidget (QList<QObject*> sourceElements)
{
	//TODO (in the inspector, too)
	return new EventInspectorWidget (static_cast<EventModel*> (sourceElements.at (0) ) );
}