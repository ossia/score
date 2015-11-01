#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    auto& constraint = static_cast<const ConstraintModel&>(sourceElement);
    return new ConstraintInspectorWidget{constraint, doc, parent};
}
