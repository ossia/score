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
    std::shared_ptr<ossia::time_constraint> ossia_cst,
    Scenario::ConstraintModel& iscore_cst,
    const Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : Execution::Component{ctx, id, "Executor::Constraint", nullptr}
    , m_iscore_constraint{iscore_cst}
    , m_ossia_constraint{std::move(ossia_cst)}
{
  ossia::time_value min_duration(Engine::iscore_to_ossia::time(
      m_iscore_constraint.duration.minDuration()));
  ossia::time_value max_duration(Engine::iscore_to_ossia::time(
      m_iscore_constraint.duration.maxDuration()));

  m_ossia_constraint->setDurationMin(min_duration);
  m_ossia_constraint->setDurationMax(max_duration);
  m_ossia_constraint->setSpeed(iscore_cst.duration.executionSpeed());

  con(iscore_cst.duration,
      &Scenario::ConstraintDurations::executionSpeedChanged, this,
      [&](double sp) { m_ossia_constraint->setSpeed(sp); });

  // BaseScenario needs a special callback.
  if (dynamic_cast<Scenario::ProcessModel*>(iscore_cst.parent())
      || dynamic_cast<Loop::ProcessModel*>(iscore_cst.parent()))
  {
    m_ossia_constraint->setCallback(
        [&](ossia::time_value position,
            ossia::time_value date,
            const ossia::state& state) {
          constraintCallback(position, date, state);
        });
  }

  for (const auto& process : iscore_cst.processes)
  {
    on_processAdded(process);
  }
}

ConstraintComponent::~ConstraintComponent()
{
  OSSIAConstraint()->setCallback(ossia::time_constraint::ExecutionCallback{});
  executionStopped();
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
    const Process::ProcessModel& iscore_proc) // TODO ProcessExecutionView
{
  // The DocumentPlugin creates the elements in the processes.
  // TODO maybe have an execution_view template on processes, that
  // gives correct const / non_const access ?
  auto proc = const_cast<Process::ProcessModel*>(&iscore_proc);
  auto fac = system().processes.factory(*proc);
  if (fac)
  {
    try
    {
      // TODO this shall go in the factory interface instead later
      // to allow the diminution of fragmentation.
      QSharedPointer<ProcessComponent> plug{fac->make(
          *this, *proc, system(), getStrongId(iscore_proc.components()), this)};
      if (plug)
      {
        m_processes.push_back(plug);
        m_ossia_constraint->addTimeProcess(plug->give_OSSIAProcess());
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
