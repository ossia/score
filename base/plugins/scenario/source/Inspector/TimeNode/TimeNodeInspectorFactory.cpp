#include "TimeNodeInspectorFactory.hpp"
#include "TimeNodeInspectorWidget.hpp"

#include <Document/TimeNode/TimeNodeModel.hpp>

//using namespace iscore;

InspectorWidgetBase* TimeNodeInspectorFactory::makeWidget (QObject* sourceElement)
{
	auto timeNode = static_cast<TimeNodeModel*>(sourceElement);
	return new TimeNodeInspectorWidget(timeNode);

}

InspectorWidgetBase* TimeNodeInspectorFactory::makeWidget (QList<QObject*> sourceElements)
{
	// @todo make a tabbed view when there is a list.
	return new TimeNodeInspectorWidget (static_cast<TimeNodeModel*> (sourceElements.at (0) ) );
}
