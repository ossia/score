#include <Loop/LoopProcessModel.hpp>

#include "LoopInspectorWidget.hpp"
#include <QVBoxLayout>

#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <Inspector/InspectorWidgetList.hpp>
#include <Process/ProcessList.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
class QVBoxLayout;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

LoopInspectorWidget::LoopInspectorWidget(
        const Loop::ProcessModel& object,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    ProcessInspectorWidgetDelegate_T {object, parent}
{
    // FIXME URGENT add implemented virtual destructors to every class that inherits from a virtual.
    auto& appContext = doc.app.components;
    auto& constraintWidgetFactory = appContext.factory<Scenario::ConstraintInspectorDelegateFactoryList>();

    auto& constraint = object.constraint();

    auto delegate = constraintWidgetFactory.make(constraint);
    if(!delegate)
        return;

    auto lay = new QVBoxLayout;
    this->setLayout(lay);
    auto& widgetFact = appContext.factory<InspectorWidgetList>();
    auto& processFact = appContext.factory<Process::ProcessList>();
    lay->addWidget(new Scenario::ConstraintInspectorWidget{
                       widgetFact,
                       processFact,
                       constraint,
                       std::move(delegate),
                       doc,
                       this});
}
