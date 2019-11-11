#include "MagnetismAdjuster.hpp"
#include <score/tools/Debug.hpp>
#include <ossia/detail/algorithms.hpp>

namespace Process
{

MagnetismAdjuster::MagnetismAdjuster() noexcept
{

}

MagnetismAdjuster::~MagnetismAdjuster() noexcept
{

}

TimeVal MagnetismAdjuster::getPosition(
    TimeVal original) noexcept
{
  // For all magnetism handlers registered,
  // find the one which is closest to the origin position
  std::vector<TimeVal> results;
  for(auto it = m_handlers.begin(); it != m_handlers.end(); )
  {
    if(it->first)
    {
      results.push_back(it->second(original));
      ++it;
    }
    else
    {
      // Cleanup in case a handler got deleted
      it = m_handlers.erase(it);
    }
  }

  // No handler -> no magnetism
  if(results.empty())
  {
    return original;
  }

  // Find the min of the distances and return the related Position
  auto it = results.begin();

  double min_distance = std::abs((original - *it).msec());
  TimeVal min_pos = *it;

  ++it;
  for(; it != results.end(); ++it)
  {
    const double d = std::abs((original - *it).msec());
    if(d < min_distance)
    {
      min_distance = d;
      min_pos = *it;
    }
  }

  return min_pos;
}

void MagnetismAdjuster::registerHandler(QObject* context, MagnetismAdjuster::MagnetismHandler h) noexcept
{
  auto it = ossia::find_if(m_handlers, [&] (auto& p) { return p.first == context; });
  if(it == m_handlers.end())
    m_handlers.emplace_back(context, h);
}

void MagnetismAdjuster::unregisterHandler(QObject* context) noexcept
{
  auto it = ossia::find_if(m_handlers, [&] (auto& p) { return p.first == context; });
  if(it != m_handlers.end())
  {
    m_handlers.erase(it);
  }
}

score::InterfaceKey MagnetismAdjuster::static_interfaceKey() noexcept
{
  return_uuid("7924a16c-089a-4659-828f-c48d5c68447a");
}
score::InterfaceKey MagnetismAdjuster::interfaceKey() const noexcept
{
  return static_interfaceKey();
}

void MagnetismAdjuster::insert(std::unique_ptr<score::InterfaceBase>) { SCORE_ABORT; }

void MagnetismAdjuster::optimize() noexcept { }

}
