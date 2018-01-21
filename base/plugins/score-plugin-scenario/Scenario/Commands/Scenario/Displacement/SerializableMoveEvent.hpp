#pragma once

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

namespace Scenario
{

class EventModel;
class ProcessModel;

namespace Command
{
class SerializableMoveEvent : public score::Command
{
public:
  ~SerializableMoveEvent();
  virtual void update(
      Scenario::ProcessModel& scenario,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double y,
      ExpandMode mode,
      LockMode lm)
      = 0;

  virtual const Path<Scenario::ProcessModel>& path() const = 0;
};
}
}
