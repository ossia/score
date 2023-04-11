#include "NetworkMetadata.hpp"

#include <Scenario/Process/ScenarioModel.hpp>

namespace Scenario
{
static constexpr std::optional<Process::NetworkFlags>
hasFlag(const Process::NetworkFlags f, Process::NetworkFlags toFind)
{
  if(f & toFind)
    return std::optional<Process::NetworkFlags>(Process::NetworkFlags(f & toFind));
  else
    return std::nullopt;
}

static auto findParentWithFlag(const auto* parent, Process::NetworkFlags flag)
{
  if(!parent)
    return Process::NetworkFlags{};

  if(auto itv = qobject_cast<const IntervalModel*>(parent))
  {
    if(auto f = hasFlag(itv->networkFlags(), flag))
    {
      return *f;
    }
    else
    {
      return findParentWithFlag(itv->parent(), flag);
    }
  }

  if(auto proc = qobject_cast<const ProcessModel*>(parent))
  {
    if(auto f = hasFlag(proc->networkFlags(), flag))
    {
      return *f;
    }
    else
    {
      return findParentWithFlag(proc->parent(), flag);
    }
  }

  return findParentWithFlag(parent->parent(), flag);
}

static Process::NetworkFlags
findNetworkFlag(const auto& p, Process::NetworkFlags flags) noexcept
{
  if(auto f = hasFlag(p.networkFlags(), flags))
    return *f;
  else
    return findParentWithFlag(&p, flags);
}

static Process::NetworkFlags findNetworkFlags(const auto& p) noexcept
{
  const auto compensated = findNetworkFlag(
      p, Process::NetworkFlags::Uncompensated | Process::NetworkFlags::Compensated);
  const auto sync
      = findNetworkFlag(p, Process::NetworkFlags::Async | Process::NetworkFlags::Sync);
  const auto share = findNetworkFlag(
      p, Process::NetworkFlags::Free | Process::NetworkFlags::Shared
             | Process::NetworkFlags::Mixed);

  return compensated | sync | share;
}

Process::NetworkFlags networkFlags(const Process::ProcessModel& p) noexcept
{
  return findNetworkFlags(p);
}

Process::NetworkFlags networkFlags(const Scenario::EventModel& p) noexcept
{
  return findNetworkFlags(p);
}

Process::NetworkFlags networkFlags(const Scenario::IntervalModel& p) noexcept
{
  return findNetworkFlags(p);
}

Process::NetworkFlags networkFlags(const Scenario::TimeSyncModel& p) noexcept
{
  return findNetworkFlags(p);
}

static QString findParentWithGroup(const QObject* parent)
{
  if(!parent)
    return QString{};

  if(auto itv = qobject_cast<const IntervalModel*>(parent))
  {
    if(auto f = itv->networkGroup(); !f.isEmpty() && f != "parent")
    {
      return f;
    }
    else
    {
      return findParentWithGroup(itv->parent());
    }
  }
  if(auto proc = qobject_cast<const ProcessModel*>(parent))
  {
    if(auto f = proc->networkGroup(); !f.isEmpty() && f != "parent")
    {
      return f;
    }
    else
    {
      return findParentWithGroup(proc->parent());
    }
  }
  return findParentWithGroup(parent->parent());
}

static QString findNetworkGroup(const auto& obj)
{
  if(auto f = obj.networkGroup(); !f.isEmpty() && f != "parent")
    return f;
  return findParentWithGroup(&obj);
}

QString networkGroup(const Process::ProcessModel& p) noexcept
{
  return findNetworkGroup(p);
}

QString networkGroup(const Scenario::EventModel& p) noexcept
{
  return findNetworkGroup(p);
}

QString networkGroup(const Scenario::IntervalModel& p) noexcept
{
  return findNetworkGroup(p);
}

QString networkGroup(const Scenario::TimeSyncModel& p) noexcept
{
  return findNetworkGroup(p);
}

}
