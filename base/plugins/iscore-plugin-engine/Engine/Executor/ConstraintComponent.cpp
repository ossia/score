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
ConstraintComponentBase::ConstraintComponentBase(
    Scenario::ConstraintModel& iscore_cst,
    const Context& ctx,
    const Id<iscore::Component>& id,
    QObject* parent)
    : Scenario::GenericConstraintComponent<const Context>{iscore_cst, ctx, id, "Executor::Constraint", nullptr}
{
  con(constraint().duration,
      &Scenario::ConstraintDurations::executionSpeedChanged, this,
      [&](double sp) { m_ossia_constraint->set_speed(sp); });

  con(constraint().duration,
      &Scenario::ConstraintDurations::defaultDurationChanged, this,
      [&](TimeVal sp) {
    system().executionQueue.enqueue([sp,cst = m_ossia_constraint]
      { cst->set_nominal_duration(iscore_to_ossia::time(sp)); });
  });

  con(constraint().duration,
      &Scenario::ConstraintDurations::minDurationChanged, this,
      [&](TimeVal sp) {
    system().executionQueue.enqueue([sp,cst = m_ossia_constraint]
      { cst->set_min_duration(iscore_to_ossia::time(sp)); });
  });

  con(constraint().duration,
      &Scenario::ConstraintDurations::maxDurationChanged, this,
      [&](TimeVal sp) {
    system().executionQueue.enqueue([sp,cst = m_ossia_constraint]
      { cst->set_max_duration(iscore_to_ossia::time(sp)); });
  });

}

ConstraintComponent::~ConstraintComponent()
{
  if(m_ossia_constraint)
    m_ossia_constraint->set_callback(ossia::time_constraint::exec_callback{});

  for(auto& proc : m_processes)
    proc.second->cleanup();
  executionStopped();
}

void ConstraintComponent::init()
{
    init_hierarchy();
}

void ConstraintComponent::cleanup()
{
  if(m_ossia_constraint)
    m_ossia_constraint->set_callback(ossia::time_constraint::exec_callback{});
  for(auto& proc : m_processes)
    proc.second->cleanup();

  clear();
  m_processes.clear();
  m_ossia_constraint.reset();
}

ConstraintComponentBase::constraint_duration_data ConstraintComponentBase::makeDurations() const
{
  return {
        iscore_to_ossia::time(constraint().duration.defaultDuration()),
        iscore_to_ossia::time(constraint().duration.minDuration()),
        iscore_to_ossia::time(constraint().duration.maxDuration()),
        constraint().duration.executionSpeed()
  };
}

void ConstraintComponent::onSetup(
    std::shared_ptr<ossia::time_constraint> ossia_cst,
    constraint_duration_data dur,
    bool parent_is_base_scenario)
{
  m_ossia_constraint = ossia_cst;

  m_ossia_constraint->set_min_duration(dur.minDuration);
  m_ossia_constraint->set_max_duration(dur.maxDuration);
  m_ossia_constraint->set_speed(dur.speed);

  // BaseScenario needs a special callback. It is given in DefaultClockManager.
  if (!parent_is_base_scenario)
  {
    m_ossia_constraint->set_stateless_callback(
        [&](ossia::time_value position,
            ossia::time_value date,
            const ossia::state_element& state) {
      auto currentTime = Engine::ossia_to_iscore::time(date);

      auto& cstdur = constraint().duration;
      const auto& maxdur = cstdur.maxDuration();

      if (!maxdur.isInfinite())
        cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
      else
        cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
    });
  }

  init();
}

std::shared_ptr<ossia::time_constraint>
ConstraintComponentBase::OSSIAConstraint() const
{
  return m_ossia_constraint;
}

Scenario::ConstraintModel& ConstraintComponentBase::iscoreConstraint() const
{
  return constraint();
}

void ConstraintComponentBase::play(TimeVal t)
{
  constraint().duration.setPlayPercentage(0);

  auto start_state = m_ossia_constraint->get_start_event().get_state();
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

void ConstraintComponentBase::pause()
{
  m_ossia_constraint->pause();
}

void ConstraintComponentBase::resume()
{
  m_ossia_constraint->resume();
}

void ConstraintComponentBase::stop()
{
  m_ossia_constraint->stop();
  auto st = m_ossia_constraint->get_end_event().get_state();
  st.launch();

  for (auto& process : m_processes)
  {
    process.second->stop();
  }
  constraint().reset();

  executionStopped();
}

void ConstraintComponentBase::executionStarted()
{
  constraint().duration.setPlayPercentage(0);
  constraint().executionStarted();
  for (Process::ProcessModel& proc : constraint().processes)
  {
    proc.startExecution();
  }
}

void ConstraintComponentBase::executionStopped()
{
  constraint().executionStopped();
  for (Process::ProcessModel& proc : constraint().processes)
  {
    proc.stopExecution();
  }
}

ProcessComponent* ConstraintComponentBase::make(
    const Id<iscore::Component> & id,
    ProcessComponentFactory& fac,
    Process::ProcessModel &proc)
{
  try
  {
    auto& self = static_cast<Engine::Execution::ConstraintComponent&>(*this);
    const Engine::Execution::Context& ctx = system();
    auto plug = fac.make(self, proc, ctx, id, nullptr);
    if (plug)
    {
      m_processes.emplace(proc.id(), plug);

      system().executionQueue.enqueue(
            [=,cst=m_ossia_constraint] {
        cst->add_time_process(plug->OSSIAProcessPtr());
      });
    }
    return plug.get();
  }
  catch (const std::exception& e)
  {
    qDebug() << "Error while creating a process: " << e.what();
  }
  catch (...)
  {
    qDebug() << "Error while creating a process";
  }
  return nullptr;
}

std::function<void ()> ConstraintComponentBase::removing(
    const Process::ProcessModel& e,
    ProcessComponent& c)
{
  auto it = m_processes.find(e.id());
  if(it != m_processes.end())
  {
    system().executionQueue.enqueue([cstr=m_ossia_constraint,proc=c.OSSIAProcessPtr()] {
      cstr->remove_time_process(proc.get());
    });

    c.cleanup();

    return [=] { m_processes.erase(it); };
  }
  return {};
}

}
}
