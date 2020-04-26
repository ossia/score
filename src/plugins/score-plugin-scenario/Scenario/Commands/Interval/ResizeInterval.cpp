#include <Process/TimeValueSerialization.hpp>
#include <Scenario/Commands/Interval/ResizeInterval.hpp>
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveEventMeta.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

namespace Scenario
{

static bool intervalHasNoFollowers(
    const Scenario::ProcessModel& scenar,
    const Scenario::IntervalModel& cst)
{
  auto& tn = Scenario::endTimeSync(cst, scenar);
  for (auto& event_id : tn.events())
  {
    Scenario::EventModel& event = scenar.events.at(event_id);
    for (auto& state_id : event.states())
    {
      Scenario::StateModel& state = scenar.states.at(state_id);
      if (state.nextInterval())
        return false;
    }
  }
  return true;
}

bool ScenarioIntervalResizer::matches(const IntervalModel& interval) const
    noexcept
{
  return dynamic_cast<Scenario::ProcessModel*>(interval.parent());
}

score::Command* ScenarioIntervalResizer::make(
    const IntervalModel& interval,
    TimeVal new_duration,
    ExpandMode e,
    LockMode l) const noexcept
{
  auto scenar = dynamic_cast<Scenario::ProcessModel*>(interval.parent());
  if (!scenar)
    return nullptr;

  // First check that the end time sync has nothing afterwards :
  // all its states must not have next intervals
  if(Scenario::isInFullView(interval))
    if(!intervalHasNoFollowers(*scenar, interval))
      return nullptr;

  auto& ev = Scenario::endState(interval, *scenar).eventId();
  auto resize_cmd
      = new Scenario::Command::MoveEventMeta{*scenar,
                                             ev,
                                             interval.date() + new_duration,
                                             interval.heightPercentage(),
                                             e,
                                             l};
  return resize_cmd;
}

void ScenarioIntervalResizer::update(
    score::Command& cmd,
    const IntervalModel& interval,
    TimeVal new_duration,
    ExpandMode e,
    LockMode l) const noexcept
{
  auto c = dynamic_cast<Scenario::Command::MoveEventMeta*>(&cmd);
  if (c)
  {
    auto scenar = dynamic_cast<Scenario::ProcessModel*>(interval.parent());
    auto& ev = Scenario::endState(interval, *scenar).eventId();
    c->update(
        *scenar,
        ev,
        interval.date() + new_duration,
        interval.heightPercentage(),
        e,
        l);
  }
}

bool BaseScenarioIntervalResizer::matches(const IntervalModel& interval) const
    noexcept
{
  return dynamic_cast<Scenario::BaseScenario*>(interval.parent());
}

score::Command* BaseScenarioIntervalResizer::make(
    const IntervalModel& interval,
    TimeVal new_duration,
    ExpandMode e,
    LockMode l) const noexcept
{
  auto scenar = dynamic_cast<Scenario::BaseScenario*>(interval.parent());
  if (!scenar)
    return nullptr;

  return new Scenario::Command::MoveBaseEvent<Scenario::BaseScenario>{
      *scenar,
      scenar->endEvent().id(),
      new_duration,
      interval.heightPercentage(),
      e,
      l};
}

void BaseScenarioIntervalResizer::update(
    score::Command& cmd,
    const IntervalModel& interval,
    TimeVal new_duration,
    ExpandMode e,
    LockMode l) const noexcept
{
  auto c = dynamic_cast<
      Scenario::Command::MoveBaseEvent<Scenario::BaseScenario>*>(&cmd);
  if (c)
  {
    auto scenar = dynamic_cast<Scenario::BaseScenario*>(interval.parent());
    auto& ev = Scenario::endState(interval, *scenar).eventId();
    c->update(
        *scenar,
        ev,
        interval.date() + new_duration,
        interval.heightPercentage(),
        e,
        l);
  }
}

IntervalResizerList::~IntervalResizerList() {}

}
