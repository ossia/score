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

const QList<QString>&ConstraintInspectorFactory::key_impl() const
{
    static const QList<QString> list{"ConstraintModel", "BaseConstraintModel"};
    return list;
}
