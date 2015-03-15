#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"

#include <Document/Constraint/ConstraintModel.hpp>

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget(QObject* sourceElement,
                                                            QWidget* parent)
{
    auto constraint = static_cast<ConstraintModel*>(sourceElement);
    return new ConstraintInspectorWidget{constraint, parent};
}
