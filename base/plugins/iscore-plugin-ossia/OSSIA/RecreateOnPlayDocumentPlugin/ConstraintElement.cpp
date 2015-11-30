#include <API/Headers/Editor/TimeConstraint.h>
#include <Automation/AutomationModel.hpp>
#include <OSSIA/OSSIA2iscore.hpp>
#include <OSSIA/RecreateOnPlayDocumentPlugin/ProcessModel/ProcessModel.hpp>
#include <OSSIA/RecreateOnPlayDocumentPlugin/ProcessModel/ProcessModelElement.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <OSSIA/iscore2OSSIA.hpp>
#include <utility>

#include "AutomationElement.hpp"
#include "ConstraintElement.hpp"
#include "Editor/TimeValue.h"
#include "Loop/LoopProcessModel.hpp"
#include "LoopElement.hpp"
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <OSSIA/RecreateOnPlayDocumentPlugin/ProcessElement.hpp>
#include <OSSIA/RecreateOnPlayDocumentPlugin/ProcessWrapper.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include "ScenarioElement.hpp"
#include <iscore/tools/SettableIdentifier.hpp>

#if defined(ISCORE_PLUGIN_MAPPING)
#include <Mapping/MappingModel.hpp>

#include "OSSIAMappingElement.hpp"
#endif

namespace RecreateOnPlay
{
ConstraintElement::ConstraintElement(
        std::shared_ptr<OSSIA::TimeConstraint> ossia_cst,
        ConstraintModel& iscore_cst,
        QObject* parent):
    QObject{parent},
    m_iscore_constraint{iscore_cst},
    m_ossia_constraint{ossia_cst}
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

ConstraintModel&ConstraintElement::iscoreConstraint() const
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
    for(Process& proc : m_iscore_constraint.processes)
    {
        proc.startExecution();
    }
}

void ConstraintElement::executionStopped()
{
    for(Process& proc : m_iscore_constraint.processes)
    {
        proc.stopExecution();
    }
}

void ConstraintElement::on_processAdded(
        const Process& iscore_proc) // TODO ProcessExecutionView
{
    // The DocumentPlugin creates the elements in the processes.
    // TODO maybe have an execution_view template on processes, that
    // gives correct const / non_const access ?
    auto proc = const_cast<Process*>(&iscore_proc);
    ProcessElement* plug{};
    if(auto scenar = dynamic_cast<Scenario::ScenarioModel*>(proc))
    {
        plug = new ScenarioElement{*this, *scenar, proc};
    }
    else if(auto autom = dynamic_cast<AutomationModel*>(proc))
    {
        plug = new AutomationElement{*this, *autom, proc};
    }
#if defined(ISCORE_PLUGIN_MAPPING)
    else if(auto mapping = dynamic_cast<MappingModel*>(proc))
    {
        plug = new MappingElement{*this, *mapping, proc};
    }
#endif
#if defined(ISCORE_PLUGIN_LOOP)
    else if(auto process = dynamic_cast<Loop::ProcessModel*>(proc))
    {
        plug = new LoopElement{*this, *process, proc};
    }
#endif
    else if(auto generic = dynamic_cast<RecreateOnPlay::OSSIAProcessModel*>(proc))
    {
        plug = new ProcessModelElement{*this, *generic, proc};
    }

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

void ConstraintElement::constraintCallback(
        const OSSIA::TimeValue& position,
        const OSSIA::TimeValue& date,
        const std::shared_ptr<OSSIA::StateElement>& state)
{
    auto currentTime = OSSIA::convert::time(date);

    auto& cstdur = m_iscore_constraint.duration;
    const auto& maxdur = cstdur.maxDuration();

    if(!maxdur.isInfinite())
        cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
    else
        cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
}
}
