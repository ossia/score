#include "IntervalInspectorFactory.hpp"
#include "IntervalInspectorWidget.hpp"

#include <Document/Interval/IntervalModel.hpp>

InspectorWidgetBase* IntervalInspectorFactory::makeWidget (QObject* sourceElement)
{
	auto interval = static_cast<IntervalModel*>(sourceElement);
	return new IntervalInspectorWidget(interval);

}

InspectorWidgetBase* IntervalInspectorFactory::makeWidget (QList<QObject*> sourceElements)
{
	// @todo make a tabbed view when there is a list.
	return new IntervalInspectorWidget (static_cast<IntervalModel*> (sourceElements.at (0) ) );
}
