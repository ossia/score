#include "DefaultClockManager.hpp"

#include <Engine/Executor/BaseScenarioElement.hpp>
#include <Engine/Executor/ConstraintElement.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Engine/OSSIA2iscore.hpp>

#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <ossia/editor/scenario/clock.hpp>
namespace Engine
{
namespace Execution
{
DefaultClockManager::~DefaultClockManager() = default;
DefaultClockManager::DefaultClockManager(const Context& ctx)
    : ClockManager{ctx}
{
  auto& bs = *ctx.sys.baseScenario();
  ossia::time_constraint& ossia_cst = *bs.baseConstraint()->OSSIAConstraint();

  ossia_cst.setDriveMode(ossia::clock::DriveMode::INTERNAL);
  ossia_cst.setGranularity(ossia::time_value(
      context.doc.app.settings<Settings::Model>().getRate()));
  ossia_cst.setCallback(makeDefaultCallback(bs));
}
ossia::time_constraint::ExecutionCallback
DefaultClockManager::makeDefaultCallback(
    Engine::Execution::BaseScenarioElement& bs)
{
  auto& cst = *bs.baseConstraint();
  return [&bs, &iscore_cst = cst.iscoreConstraint() ](
      ossia::time_value position,
      ossia::time_value date,
      const ossia::state_element& state)
  {
    ossia::launch(state);

    auto currentTime = Engine::ossia_to_iscore::time(date);

    auto& cstdur = iscore_cst.duration;
    const auto& maxdur = cstdur.maxDuration();

    if (!maxdur.isInfinite())
      cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
    else
      cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
  };
}

void DefaultClockManager::play_impl(
    const TimeValue& t, BaseScenarioElement& bs)
{
  bs.baseConstraint()->play(t);
}

void DefaultClockManager::pause_impl(BaseScenarioElement& bs)
{
  bs.baseConstraint()->pause();
}

void DefaultClockManager::resume_impl(BaseScenarioElement& bs)
{
  bs.baseConstraint()->resume();
}

void DefaultClockManager::stop_impl(BaseScenarioElement& bs)
{
  bs.baseConstraint()->stop();
}

DefaultClockManagerFactory::~DefaultClockManagerFactory() = default;
QString DefaultClockManagerFactory::prettyName() const
{
  return QObject::tr("Default");
}

std::unique_ptr<ClockManager>
DefaultClockManagerFactory::make(const Engine::Execution::Context& ctx)
{
  return std::make_unique<DefaultClockManager>(ctx);
}
}
}
