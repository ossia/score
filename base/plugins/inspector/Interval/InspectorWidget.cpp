#include "InspectorWidget.hpp"
#include <Interval/IntervalInspectorview.hpp>
#include <QDebug>

//using namespace iscore;

InspectorWidgetInterface* InspectorWidget::makeWidget (QObject* sourceElement)
{
	if (QString (sourceElement->metaObject()->className() ).compare ("ObjectInterval") == 0)
	{
		return new IntervalInspectorView (static_cast<ObjectInterval*> (sourceElement) );
	}
	else
	{
		return new InspectorWidgetInterface (sourceElement);
	}
}

InspectorWidgetInterface* InspectorWidget::makeWidget (QList<QObject*> sourceElements)
{
	return new IntervalInspectorView (static_cast<ObjectInterval*> (sourceElements.at (0) ) );
}
