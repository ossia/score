// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
  auto& bs = context.scenario;
  play_impl(t, bs);
}

void ClockManager::pause()
{
  auto& bs = context.scenario;
  pause_impl(bs);
}

void ClockManager::resume()
{
  auto& bs = context.scenario;
  resume_impl(bs);
}

void ClockManager::stop()
{
  auto& bs = context.scenario;
  if(bs.active())
    stop_impl(bs);
}

bool ClockManager::paused() const
{
  return false;
}
}
}
