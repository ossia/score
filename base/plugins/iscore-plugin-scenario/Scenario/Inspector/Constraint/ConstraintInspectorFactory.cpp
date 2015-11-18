#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <core/document/Document.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <Process/ProcessList.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    auto& appContext = doc.context().app;
    auto& widgetFact = appContext.components.factory<InspectorWidgetList>();
    auto& processFact = appContext.components.factory<DynamicProcessList>();
    auto& constraintWidgetFactory = appContext.components.factory<ConstraintInspectorDelegateFactoryList>();

    // make a relevant widget delegate :

    auto& constraint = static_cast<const ConstraintModel&>(sourceElement);

    return new ConstraintInspectorWidget{widgetFact, processFact, constraint, constraintWidgetFactory.make(constraint),doc, parent};
}

const QList<QString>&ConstraintInspectorFactory::key_impl() const
{
    static const QList<QString> list{"ConstraintModel", "BaseConstraintModel"};
    return list;
}
