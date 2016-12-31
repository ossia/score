#include <ossia/editor/scenario/time_constraint.hpp>
#include <Automation/AutomationModel.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <utility>

#include "ConstraintComponent.hpp"
#include "Loop/LoopProcessModel.hpp"
#include "ScenarioComponent.hpp"
#include <ossia/editor/scenario/time_value.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>

namespace Engine
{
namespace Execution
{
ConstraintComponent::ConstraintComponent(
    Scenario::ConstraintModel& iscore_cst,
    const Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : Execution::Component{ctx, id, "Executor::Constraint", nullptr}
    , m_iscore_constraint{iscore_cst}
{
  con(m_iscore_constraint.duration,
      &Scenario::ConstraintDurations::executionSpeedChanged, this,
      [&](double sp) { m_ossia_constraint->setSpeed(sp); });
  con(m_iscore_constraint.duration,
      &Scenario::ConstraintDurations::defaultDurationChanged, this,
      [&](TimeValue sp) {
    system().executionQueue.enqueue([sp,cst = m_ossia_constraint]
      { cst->setDurationNominal(iscore_to_ossia::time(sp)); });
  });
  con(m_iscore_constraint.duration,
      &Scenario::ConstraintDurations::minDurationChanged, this,
      [&](TimeValue sp) {
    system().executionQueue.enqueue([sp,cst = m_ossia_constraint]
      { cst->setDurationMin(iscore_to_ossia::time(sp)); });
  });
  con(m_iscore_constraint.duration,
      &Scenario::ConstraintDurations::maxDurationChanged, this,
      [&](TimeValue sp) {
    system().executionQueue.enqueue([sp,cst = m_ossia_constraint]
      { cst->setDurationMax(iscore_to_ossia::time(sp)); });
  });

}

ConstraintComponent::~ConstraintComponent()
{
  if(m_ossia_constraint)
    m_ossia_constraint->setCallback(ossia::time_constraint::ExecutionCallback{});

  for(auto& proc : m_processes)
    proc->cleanup();
  executionStopped();
}

void ConstraintComponent::init()
{
  for (auto& process : m_iscore_constraint.processes)
  {
    on_processAdded(process);
  }
}

void ConstraintComponent::cleanup()
{
  m_ossia_constraint->setCallback(ossia::time_constraint::ExecutionCallback{});
  for(auto& proc : m_processes)
    proc->cleanup();

  m_processes.clear();
  m_ossia_constraint.reset();
}

ConstraintComponent::constraint_duration_data ConstraintComponent::makeDurations() const
{
  return {
        iscore_to_ossia::time(m_iscore_constraint.duration.defaultDuration()),
        iscore_to_ossia::time(m_iscore_constraint.duration.minDuration()),
        iscore_to_ossia::time(m_iscore_constraint.duration.maxDuration()),
        m_iscore_constraint.duration.executionSpeed()
  };
}

void ConstraintComponent::onSetup(
    std::shared_ptr<ossia::time_constraint> ossia_cst,
    constraint_duration_data dur,
    bool parent_is_base_scenario)
{
  m_ossia_constraint = ossia_cst;

  m_ossia_constraint->setDurationMin(dur.minDuration);
  m_ossia_constraint->setDurationMax(dur.maxDuration);
  m_ossia_constraint->setSpeed(dur.speed);

  // BaseScenario needs a special callback.
  if (!parent_is_base_scenario)
  {
    m_ossia_constraint->setCallback(
        [&](ossia::time_value position,
            ossia::time_value date,
            const ossia::state& state) {
          constraintCallback(position, date, state);
        });
  }

  init();
}

std::shared_ptr<ossia::time_constraint>
ConstraintComponent::OSSIAConstraint() const
{
  return m_ossia_constraint;
}

Scenario::ConstraintModel& ConstraintComponent::iscoreConstraint() const
{
  return m_iscore_constraint;
}

void ConstraintComponent::play(TimeValue t)
{
  m_iscore_constraint.duration.setPlayPercentage(0);

  auto start_state = m_ossia_constraint->getStartEvent().getState();
  auto offset_state = m_ossia_constraint->offset(Engine::iscore_to_ossia::time(t));

  ossia::state accumulator;
  ossia::flatten_and_filter(accumulator, start_state);
  ossia::flatten_and_filter(accumulator, offset_state);
  accumulator.launch();

  try
  {
    m_ossia_constraint->start();
    executionStarted();
  }
  catch (const std::exception& e)
  {
    qDebug() << e.what();
  }
}

void ConstraintComponent::pause()
{
  m_ossia_constraint->pause();
}

void ConstraintComponent::resume()
{
  m_ossia_constraint->resume();
}

void ConstraintComponent::stop()
{
  m_ossia_constraint->stop();
  auto st = m_ossia_constraint->getEndEvent().getState();
  st.launch();

  for (auto& process : m_processes)
  {
    process->stop();
  }
  m_iscore_constraint.reset();

  executionStopped();
}

void ConstraintComponent::executionStarted()
{
  m_iscore_constraint.duration.setPlayPercentage(0);
  m_iscore_constraint.executionStarted();
  for (Process::ProcessModel& proc : m_iscore_constraint.processes)
  {
    proc.startExecution();
  }
}

void ConstraintComponent::executionStopped()
{
  m_iscore_constraint.executionStopped();
  for (Process::ProcessModel& proc : m_iscore_constraint.processes)
  {
    proc.stopExecution();
  }
}

void ConstraintComponent::on_processAdded(
    Process::ProcessModel& proc)
{
  auto fac = system().processes.factory(proc);
  if (fac)
  {
    try
    {
      auto plug = fac->make(
          *this, proc, system(), iscore::newId(proc), this);
      if (plug)
      {
        m_processes.push_back(plug);

        system().executionQueue.enqueue([cst=m_ossia_constraint,proc=plug] {
          cst->addTimeProcess(proc->OSSIAProcessPtr());
        });
      }
    }
    catch (const std::exception& e)
    {
      qDebug() << "Error while creating a process: " << e.what();
    }
    catch (...)
    {
      qDebug() << "Error while creating a process";
    }
  }
}

void ConstraintComponent::constraintCallback(
    ossia::time_value position,
    ossia::time_value date,
    const ossia::state& state)
{
  auto currentTime = Engine::ossia_to_iscore::time(date);

  auto& cstdur = m_iscore_constraint.duration;
  const auto& maxdur = cstdur.maxDuration();

  if (!maxdur.isInfinite())
    cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
  else
    cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
}
}
}
