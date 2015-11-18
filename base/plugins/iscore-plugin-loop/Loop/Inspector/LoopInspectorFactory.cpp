#include "LoopInspectorFactory.hpp"
#include "LoopInspectorWidget.hpp"
#include <Loop/LoopProcessModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <Process/ProcessList.hpp>
#include <core/document/Document.hpp>
#include <core/application/ApplicationComponents.hpp>


LoopInspectorFactory::LoopInspectorFactory() :
    InspectorWidgetFactory {}
{

}

LoopInspectorFactory::~LoopInspectorFactory()
{

}

InspectorWidgetBase* LoopInspectorFactory::makeWidget(
        const QObject& sourceElement,
        iscore::Document& doc,
        QWidget* parent)
{
    auto& appContext = doc.context().app;
    auto& widgetFact = appContext.components.factory<InspectorWidgetList>();
    auto& processFact = appContext.components.factory<DynamicProcessList>();

    auto& constraint = static_cast<const LoopProcessModel&>(sourceElement).baseConstraint();
    return new ConstraintInspectorWidget{widgetFact, processFact, constraint, doc, parent};

}
