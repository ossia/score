#include "ClockManagerFactory.hpp"
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/ExecutorContext.hpp>
namespace Engine
{
namespace Execution
{

ClockManager::~ClockManager() = default;
ClockManagerFactory::~ClockManagerFactory() = default;

void ClockManager::play(const TimeVal& t)
{
  auto bs = context.sys.baseScenario();
  if (!bs)
    return;

  play_impl(t, *bs);
}

void ClockManager::pause()
{
  auto bs = context.sys.baseScenario();
  if (!bs)
    return;

  pause_impl(*bs);
}

void ClockManager::resume()
{
  auto bs = context.sys.baseScenario();
  if (!bs)
    return;

  resume_impl(*bs);
}

void ClockManager::stop()
{
  auto bs = context.sys.baseScenario();
  if (!bs)
    return;

  stop_impl(*bs);
}
}
}
