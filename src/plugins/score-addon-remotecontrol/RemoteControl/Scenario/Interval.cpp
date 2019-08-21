#include "Interval.hpp"
namespace RemoteControl
{
IntervalBase::IntervalBase(
    const Id<score::Component>& id,
    Scenario::IntervalModel& Interval,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : parent_t{Interval, doc, id, "IntervalComponent", parent_comp}
{
}

ProcessComponent* IntervalBase::make(
    const Id<score::Component>& id,
    ProcessComponentFactory& factory,
    Process::ProcessModel& process)
{
  return factory.make(process, system(), id, this);
}

bool IntervalBase::removing(
    const Process::ProcessModel& cst,
    const ProcessComponent& comp)
{
  return true;
}

}
