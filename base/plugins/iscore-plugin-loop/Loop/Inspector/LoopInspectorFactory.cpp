#include "LoopInspectorFactory.hpp"
#include "LoopInspectorWidget.hpp"
#include <Loop/LoopProcessModel.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>

#include <Inspector/InspectorWidgetList.hpp>

#include <Process/ProcessList.hpp>

#include <core/document/Document.hpp>
#include <core/application/ApplicationComponents.hpp>

class LoopConstraintInspectorDelegate final : public ConstraintInspectorDelegate
{
    public:
        LoopConstraintInspectorDelegate(const ConstraintModel& cst);

        void updateElements() override;
        void addWidgets_pre(std::list<QWidget*>& widgets, ConstraintInspectorWidget* parent) override;
        void addWidgets_post(std::list<QWidget*>& widgets, ConstraintInspectorWidget* parent) override;

        void on_defaultDurationChanged(
                OngoingCommandDispatcher& dispatcher,
                const TimeValue& val,
                ExpandMode) const override;
};


LoopConstraintInspectorDelegate::LoopConstraintInspectorDelegate(
        const ConstraintModel& cst):
    ConstraintInspectorDelegate{cst}
{
}

void LoopConstraintInspectorDelegate::updateElements()
{
}

void LoopConstraintInspectorDelegate::addWidgets_pre(
        std::list<QWidget*>& widgets,
        ConstraintInspectorWidget* parent)
{
}

void LoopConstraintInspectorDelegate::addWidgets_post(
        std::list<QWidget*>& widgets,
        ConstraintInspectorWidget* parent)
{
}

void LoopConstraintInspectorDelegate::on_defaultDurationChanged(
        OngoingCommandDispatcher& dispatcher,
        const TimeValue& val,
        ExpandMode expandmode) const
{
    /*
    auto Loop = m_model.parentLoop();
    dispatcher.submitCommand<MoveEventMeta>(
            *safe_cast<LoopModel*>(m_model.parent()),
            Loop->state(m_model.endState()).eventId(),
            m_model.startDate() + val,
            expandmode);
            */
}





LoopConstraintInspectorDelegateFactory::~LoopConstraintInspectorDelegateFactory()
{

}

std::unique_ptr<ConstraintInspectorDelegate> LoopConstraintInspectorDelegateFactory::make(
        const ConstraintModel& constraint)
{
    return std::make_unique<LoopConstraintInspectorDelegate>(constraint);
}

bool LoopConstraintInspectorDelegateFactory::matches(
        const ConstraintModel& constraint) const
{
    return dynamic_cast<LoopProcessModel*>(constraint.parent());
}




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
    auto& constraintWidgetFactory = appContext.components.factory<ConstraintInspectorDelegateFactoryList>();

    auto& constraint = static_cast<const LoopProcessModel&>(sourceElement).baseConstraint();

    auto delegate = constraintWidgetFactory.make(constraint);
    if(!delegate)
        return nullptr;

    return new ConstraintInspectorWidget{widgetFact, processFact, constraint, std::move(delegate), doc, parent};

}
