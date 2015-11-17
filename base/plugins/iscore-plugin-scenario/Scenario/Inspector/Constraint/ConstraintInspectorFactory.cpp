#include "ConstraintInspectorFactory.hpp"
#include "ConstraintInspectorWidget.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <core/document/Document.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/application/ApplicationComponents.hpp>
#include <Process/ProcessList.hpp>

InspectorWidgetBase* ConstraintInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    auto& appContext = doc.context().app;
    auto& widgetFact = appContext.components.factory<InspectorWidgetList>();
    auto& processFact = appContext.components.factory<DynamicProcessList>();

    auto& constraint = static_cast<const ConstraintModel&>(sourceElement);
    return new ConstraintInspectorWidget{widgetFact, processFact, constraint, doc, parent};
}

const QList<QString>&ConstraintInspectorFactory::key_impl() const
{
    static const QList<QString> list{"ConstraintModel", "BaseConstraintModel"};
    return list;
}
