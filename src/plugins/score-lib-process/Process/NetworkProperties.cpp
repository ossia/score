#include "NetworkProperties.hpp"

#include <Process/Process.hpp>

namespace Process
{
NetworkProperties::NetworkProperties() = default;
NetworkProperties::~NetworkProperties() = default;

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

  if(auto itv = dynamic_cast<const NetworkProperties*>(parent))
  {
    if(auto f = hasFlag(itv->networkFlags(), flag))
    {
      return *f;
    }
    else
    {
      return findParentWithFlag(parent->parent(), flag);
    }
  }
  else
  {
    return findParentWithFlag(parent->parent(), flag);
  }
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
  const auto active
      = (Process::NetworkFlags)(p.networkFlags() & Process::NetworkFlags::Active);

  return compensated | sync | share | active;
}

Process::NetworkFlags networkFlags(const Process::ProcessModel& p) noexcept
{
  return findNetworkFlags(p);
}

static QString findParentWithGroup(const QObject* parent)
{
  if(!parent)
    return QString{};

  if(auto itv = dynamic_cast<const NetworkProperties*>(parent))
  {
    if(auto f = itv->networkGroup(); !f.isEmpty() && f != "parent")
    {
      return f;
    }
    else
    {
      return findParentWithGroup(parent->parent());
    }
  }
  else
  {
    return findParentWithGroup(parent->parent());
  }
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

}