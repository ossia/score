#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"

#include <Document/Constraint/ConstraintModel.hpp>

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget(
        const QObject& sourceElement,
        QWidget* parent)
{
    auto& constraint = static_cast<const ConstraintModel&>(sourceElement);
    return new ConstraintInspectorWidget{constraint, parent};
}
