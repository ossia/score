// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalComponent.hpp"
#include <ossia/detail/algorithms.hpp>
#include <ossia/editor/state/state_element.hpp>

namespace Engine
{
namespace LocalTree
{
IntervalBase::IntervalBase(
    ossia::net::node_base& parent,
    const Id<score::Component>& id,
    Scenario::IntervalModel& interval,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : parent_t{parent, interval.metadata(), interval, doc,
               id,     "IntervalComponent", parent_comp}
    , m_processesNode{*node().create_child("processes")}
{
  using namespace Scenario;
  using tv_t = ::TimeVal;

  m_properties.push_back(add_property<float>(
      node(), "yPos", &interval, &IntervalModel::heightPercentage,
      &IntervalModel::setHeightPercentage,
      &IntervalModel::heightPercentageChanged, this));

  m_properties.push_back(add_getProperty<tv_t>(
      node(), "min", &interval.duration, &IntervalDurations::minDuration,
      &IntervalDurations::minDurationChanged, this));

  m_properties.push_back(add_getProperty<tv_t>(
      node(), "max", &interval.duration, &IntervalDurations::maxDuration,
      &IntervalDurations::maxDurationChanged, this));

  m_properties.push_back(add_getProperty<tv_t>(
      node(), "default", &interval.duration,
      &IntervalDurations::defaultDuration,
      &IntervalDurations::defaultDurationChanged, this));

  m_properties.push_back(add_getProperty<float>(
      node(), "playtime", &interval.duration,
      &IntervalDurations::playPercentage,
      &IntervalDurations::playPercentageChanged, this));

  m_properties.push_back(add_property<double>(
      node(), "speed", &interval.duration,
      &IntervalDurations::executionSpeed,
      &IntervalDurations::setExecutionSpeed,
      &IntervalDurations::executionSpeedChanged, this));
}

ProcessComponent* IntervalBase::make(
    const Id<score::Component>& id,
    ProcessComponentFactory& factory,
    Process::ProcessModel& process)
{
  return factory.make(id, m_processesNode, process, system(), this);
}

bool IntervalBase::removing(
    const Process::ProcessModel& cst, const ProcessComponent& comp)
{
  return true;
}
}
}
