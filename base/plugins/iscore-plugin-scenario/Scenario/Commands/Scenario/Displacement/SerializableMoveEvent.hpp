#pragma once

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

namespace Scenario
{

class EventModel;
class ProcessModel;

namespace Command
{
class SerializableMoveEvent : public iscore::Command
{
public:
  virtual void update(
      Scenario::ProcessModel& scenario,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double y,
      ExpandMode mode)
      = 0;

  virtual const Path<Scenario::ProcessModel>& path() const = 0;
};
}
}
