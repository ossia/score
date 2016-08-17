#include <ossia/editor/scenario/time_constraint.hpp>
#include <Automation/AutomationModel.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <utility>

#include "ConstraintElement.hpp"
#include <ossia/editor/scenario/time_value.hpp>
#include "Loop/LoopProcessModel.hpp"
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Engine/Executor/ProcessElement.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include "ScenarioElement.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <iscore/document/DocumentContext.hpp>

namespace Engine { namespace Execution
{
ConstraintElement::ConstraintElement(
        std::shared_ptr<ossia::time_constraint> ossia_cst,
        Scenario::ConstraintModel& iscore_cst,
        const Context&ctx,
        QObject* parent):
    QObject{parent},
    m_iscore_constraint{iscore_cst},
    m_ossia_constraint{ossia_cst},
    m_ctx{ctx}
{
    ossia::time_value min_duration(Engine::iscore_to_ossia::time(m_iscore_constraint.duration.minDuration()));
    ossia::time_value max_duration(Engine::iscore_to_ossia::time(m_iscore_constraint.duration.maxDuration()));

    m_ossia_constraint->setDurationMin(min_duration);
    m_ossia_constraint->setDurationMax(max_duration);
    m_ossia_constraint->setSpeed(iscore_cst.duration.executionSpeed());

    con(iscore_cst.duration, &Scenario::ConstraintDurations::executionSpeedChanged,
        this, [&] (double sp) {
        m_ossia_constraint->setSpeed(sp);
    });

    // BaseScenario needs a special callback.
    if(dynamic_cast<Scenario::ProcessModel*>(iscore_cst.parent())
            || dynamic_cast<Loop::ProcessModel*>(iscore_cst.parent()))
    {
        ossia_cst->setCallback([&] (
                               ossia::time_value position,
                               ossia::time_value date,
                               const ossia::state& state)
        {
            constraintCallback(position, date, state);
        });
    }

    for(const auto& process : iscore_cst.processes)
    {
        on_processAdded(process);
    }

}

std::shared_ptr<ossia::time_constraint> ConstraintElement::OSSIAConstraint() const
{
    return m_ossia_constraint;
}

Scenario::ConstraintModel& ConstraintElement::iscoreConstraint() const
{
    return m_iscore_constraint;
}

void ConstraintElement::play(TimeValue t)
{
    m_offset = Engine::iscore_to_ossia::time(t);
    m_iscore_constraint.duration.setPlayPercentage(0);

    auto start_state = m_ossia_constraint->getStartEvent().getState();
    auto offset_state = m_ossia_constraint->offset(m_offset);

    ossia::state accumulator;
    ossia::flatten_and_filter(accumulator, start_state);
    ossia::flatten_and_filter(accumulator, offset_state);
    accumulator.launch();

    try {
        m_ossia_constraint->start();
        executionStarted();
    }
    catch(const std::exception& e) {
        qDebug() << e.what();
    }

}

void ConstraintElement::pause()
{
    m_ossia_constraint->pause();
}

void ConstraintElement::resume()
{
    m_ossia_constraint->resume();
}

void ConstraintElement::stop()
{
    m_ossia_constraint->stop();
    auto st = m_ossia_constraint->getEndEvent().getState();
    st.launch();

    for(auto& process : m_processes)
    {
        process->stop();
    }
    m_iscore_constraint.reset();

    executionStopped();
}

void ConstraintElement::executionStarted()
{
    m_iscore_constraint.duration.setPlayPercentage(0);
    m_iscore_constraint.executionStarted();
    for(Process::ProcessModel& proc : m_iscore_constraint.processes)
    {
        proc.startExecution();
    }
}

void ConstraintElement::executionStopped()
{
    m_iscore_constraint.executionStopped();
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
    auto fac = m_ctx.processes.factory(*proc);
    if(fac)
    {
        auto plug = fac->make(*this, *proc, m_ctx, getStrongId(iscore_proc.components), this);
        if(plug)
        {
            m_processes.push_back(plug);
            m_ossia_constraint->addTimeProcess(plug->give_OSSIAProcess());
        }
    }
}

void ConstraintElement::constraintCallback(
        ossia::time_value position,
        ossia::time_value date,
        const ossia::state& state)
{
    auto currentTime = Engine::ossia_to_iscore::time(date);

    auto& cstdur = m_iscore_constraint.duration;
    const auto& maxdur = cstdur.maxDuration();

    if(!maxdur.isInfinite())
        cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
    else
        cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
}
} }
