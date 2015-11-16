#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <core/document/Document.hpp>
#include <Inspector/InspectorWidgetList.hpp>

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    auto& appContext = doc.context().app;
    auto fact = appContext.components.factory<InspectorWidgetList>();
    if(fact)
    {
        auto& constraint = static_cast<const ConstraintModel&>(sourceElement);
        return new ConstraintInspectorWidget{*fact, constraint, doc, parent};
    }
    else
    {
        return nullptr;
    }
}

const QList<QString>&ConstraintInspectorFactory::key_impl() const
{
    static const QList<QString> list{"ConstraintModel", "BaseConstraintModel"};
    return list;
}
