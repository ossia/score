#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"

#include <Document/Constraint/ConstraintModel.hpp>

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget (QObject* sourceElement)
{
	auto constraint = static_cast<ConstraintModel*>(sourceElement);
	return new ConstraintInspectorWidget(constraint);

}

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget (QList<QObject*> sourceElements)
{
	// @todo make a tabbed view when there is a list.
	return new ConstraintInspectorWidget (static_cast<ConstraintModel*> (sourceElements.at (0) ) );
}
