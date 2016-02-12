#include <Inspector/InspectorWidgetList.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>

#include <boost/optional/optional.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QtGlobal>
#include <QObject>
#include <algorithm>
#include <list>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "LoopInspectorFactory.hpp"
#include "LoopInspectorWidget.hpp"
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>

class InspectorWidgetBase;
class QWidget;

// TODO cleanup this file
namespace Loop
{
class ConstraintInspectorDelegate final : public Scenario::ConstraintInspectorDelegate
{
    public:
        ConstraintInspectorDelegate(const Scenario::ConstraintModel& cst);

        void updateElements() override;
        void addWidgets_pre(std::list<QWidget*>& widgets, Scenario::ConstraintInspectorWidget* parent) override;
        void addWidgets_post(std::list<QWidget*>& widgets, Scenario::ConstraintInspectorWidget* parent) override;

        void on_defaultDurationChanged(
                OngoingCommandDispatcher& dispatcher,
                const TimeValue& val,
                ExpandMode) const override;
};


ConstraintInspectorDelegate::ConstraintInspectorDelegate(
        const Scenario::ConstraintModel& cst):
    Scenario::ConstraintInspectorDelegate{cst}
{
}

void ConstraintInspectorDelegate::updateElements()
{
}

void ConstraintInspectorDelegate::addWidgets_pre(
        std::list<QWidget*>& widgets,
        Scenario::ConstraintInspectorWidget* parent)
{
}

void ConstraintInspectorDelegate::addWidgets_post(
        std::list<QWidget*>& widgets,
        Scenario::ConstraintInspectorWidget* parent)
{
}

void ConstraintInspectorDelegate::on_defaultDurationChanged(
        OngoingCommandDispatcher& dispatcher,
        const TimeValue& val,
        ExpandMode expandmode) const
{
    auto& loop = *safe_cast<Loop::ProcessModel*>(m_model.parent());
    dispatcher.submitCommand<Scenario::Command::MoveBaseEvent<Loop::ProcessModel>>(
            loop,
            loop.state(m_model.endState()).eventId(),
            m_model.startDate() + val,
            0,
            expandmode);
}





ConstraintInspectorDelegateFactory::~ConstraintInspectorDelegateFactory()
{

}

std::unique_ptr<Scenario::ConstraintInspectorDelegate> ConstraintInspectorDelegateFactory::make(
        const Scenario::ConstraintModel& constraint)
{
    return std::make_unique<Loop::ConstraintInspectorDelegate>(constraint);
}

bool ConstraintInspectorDelegateFactory::matches(
        const Scenario::ConstraintModel& constraint) const
{
    return dynamic_cast<Loop::ProcessModel*>(constraint.parent());
}



InspectorFactory::InspectorFactory() :
    ProcessInspectorWidgetDelegateFactory {}
{

}

InspectorFactory::~InspectorFactory()
{

}

ProcessInspectorWidgetDelegate* InspectorFactory::make(
        const Process::ProcessModel& process,
        const iscore::DocumentContext& doc,
        QWidget* parent) const
{
    return new LoopInspectorWidget{
        static_cast<const Loop::ProcessModel&>(process),
                doc,
                parent};
}

bool InspectorFactory::matches(const Process::ProcessModel& process) const
{
    return dynamic_cast<const Loop::ProcessModel*>(&process);
}
}
