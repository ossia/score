#include "IntervalInspectorFactory.hpp"
#include "IntervalInspectorWidget.hpp"

#include <Document/Interval/IntervalModel.hpp>
//#include <QDebug>

//using namespace iscore;

InspectorWidgetBase* IntervalInspectorFactory::makeWidget (QObject* sourceElement)
{
	auto interval = static_cast<IntervalModel*>(sourceElement);
	return new IntervalInspectorWidget(interval);
	
}

InspectorWidgetBase* IntervalInspectorFactory::makeWidget (QList<QObject*> sourceElements)
{
	//TODO (in the inspector, too)
	return new IntervalInspectorWidget (static_cast<IntervalModel*> (sourceElements.at (0) ) );
}
