// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalComponent.hpp"

#include <ossia/detail/algorithms.hpp>


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
    : parent_t{parent, interval.metadata(), interval,   doc,
               id, "IntervalComponent", parent_comp}
    , m_processesNode{*node().create_child("processes")}
{
  using namespace Scenario;

  add_get<IntervalDurations::p_min>(interval.duration);
  add_get<IntervalDurations::p_max>(interval.duration);
  add_get<IntervalDurations::p_default>(interval.duration);
  add_get<IntervalDurations::p_percentage>(interval.duration);

  add<IntervalDurations::p_speed>(interval.duration);
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
