#pragma once

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

namespace Scenario
{

class EventModel;
class ProcessModel;

namespace Command
{
class SerializableMoveEvent : public iscore::SerializableCommand
{
public:
  virtual void update(
      const Id<EventModel>& eventId,
      const TimeValue& newDate,
      double y,
      ExpandMode mode)
      = 0;

  virtual const Path<Scenario::ProcessModel>& path() const = 0;
};
}
}
