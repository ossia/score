#include <API/Headers/Editor/TimeConstraint.h>
#include <Automation/AutomationModel.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <utility>

#include "ConstraintElement.hpp"
#include "Editor/TimeValue.h"
#include "Loop/LoopProcessModel.hpp"
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ProcessWrapper.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include "ScenarioElement.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <iscore/document/DocumentContext.hpp>

namespace RecreateOnPlay
{
ConstraintElement::ConstraintElement(
        std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
        Scenario::ConstraintModel& iscore_cst,
        const Context&ctx,
        QObject* parent):
    QObject{parent},
    m_iscore_constraint{iscore_cst},
    m_ossia_constraint{ossia_cst},
    m_ctx{ctx}
{
    OSSIA::TimeValue min_duration(iscore::convert::time(m_iscore_constraint.duration.minDuration()));
    OSSIA::TimeValue max_duration(iscore::convert::time(m_iscore_constraint.duration.maxDuration()));

    m_ossia_constraint->setDurationMin(min_duration);
    m_ossia_constraint->setDurationMax(max_duration);

    // BaseScenario needs a special callback.
    if(dynamic_cast<Scenario::ScenarioModel*>(iscore_cst.parent())
    || dynamic_cast<Loop::ProcessModel*>(iscore_cst.parent()))
    {
        ossia_cst->setCallback([&] (
                               const OSSIA::TimeValue& position,
                               const OSSIA::TimeValue& date,
                               const std::shared_ptr<OSSIA::StateElement>& state)
        {
            constraintCallback(position, date, state);
        });
    }

    for(const auto& process : iscore_cst.processes)
    {
        on_processAdded(process);
    }
}

std::shared_ptr<OSSIA::TimeConstraint> ConstraintElement::OSSIAConstraint() const
{
    return m_ossia_constraint;
}

Scenario::ConstraintModel& ConstraintElement::iscoreConstraint() const
{
    return m_iscore_constraint;
}

void ConstraintElement::play(TimeValue t)
{
    m_offset = iscore::convert::time(t);
    m_iscore_constraint.duration.setPlayPercentage(0);
    m_ossia_constraint->setOffset(m_offset);
    m_ossia_constraint->start();
    executionStarted();
}

void ConstraintElement::stop()
{
    m_ossia_constraint->stop();
    for(auto& process : m_processes)
    {
        process.second.element->stop();
    }

    m_iscore_constraint.reset();
    executionStopped();
}

void ConstraintElement::executionStarted()
{
    m_iscore_constraint.duration.setPlayPercentage(0);
    for(Process::ProcessModel& proc : m_iscore_constraint.processes)
    {
        proc.startExecution();
    }
}

void ConstraintElement::executionStopped()
{
    for(Process::ProcessModel& proc : m_iscore_constraint.processes)
    {
        proc.stopExecution();
    }
}

void ConstraintElement::on_processAdded(
        const Process::ProcessModel& iscore_proc) // TODO ProcessExecutionView
{
    // The DocumentPlugin creates the elements in the processes.
    // TODO maybe have an execution_view template on processes, that
    // gives correct const / non_const access ?
    auto proc = const_cast<Process::ProcessModel*>(&iscore_proc);
    auto fac = m_ctx.processes.factory(*proc, m_ctx.sys, m_ctx.doc);
    if(fac)
    {
        auto plug = fac->make(*this, *proc, m_ctx, getStrongId(iscore_proc.components), this);
        if(plug)
        {
            auto id = iscore_proc.id();
            m_processes.insert(std::make_pair(id,
                                OSSIAProcess(plug,
                                 std::make_unique<ProcessWrapper>(
                                     m_ossia_constraint,
                                     plug->OSSIAProcess(),
                                     iscore::convert::time(plug->iscoreProcess().duration()),
                                     m_iscore_constraint.looping()
                                 )
                                )
                               ));
        }
    }
}

void ConstraintElement::constraintCallback(
        const OSSIA::TimeValue& position,
        const OSSIA::TimeValue& date,
        const std::shared_ptr<OSSIA::StateElement>& state)
{
    auto currentTime = Ossia::convert::time(date);

    auto& cstdur = m_iscore_constraint.duration;
    const auto& maxdur = cstdur.maxDuration();

    if(!maxdur.isInfinite())
        cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
    else
        cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
}
}
