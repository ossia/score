#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"

#include <Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp>

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget (QObject* sourceElement)
{
    auto constraint = static_cast<TemporalConstraintViewModel*> (sourceElement);
    return new ConstraintInspectorWidget (constraint);

}

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget (QList<QObject*> sourceElements)
{
    return new ConstraintInspectorWidget (static_cast<TemporalConstraintViewModel*> (sourceElements.at (0) ) );
}
