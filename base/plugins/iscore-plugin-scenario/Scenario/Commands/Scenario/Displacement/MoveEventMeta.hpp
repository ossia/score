#pragma once

#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>

#include "MoveEventFactoryInterface.hpp"
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <iscore/tools/SettableIdentifier.hpp>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class ProcessModel;
namespace Command
{

class ISCORE_PLUGIN_SCENARIO_EXPORT MoveEventMeta final
    : public SerializableMoveEvent
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), MoveEventMeta, "Move an event")

public:
  MoveEventMeta(
      Path<Scenario::ProcessModel>&& scenarioPath,
      Id<EventModel>
          eventId,
      TimeValue newDate,
      double y,
      ExpandMode mode);

  void undo() const override;
  void redo() const override;

  const Path<Scenario::ProcessModel>& path() const override;

  void update(
      const Id<EventModel>& eventId, const TimeValue& newDate, double y,
      ExpandMode mode) override;
  void update(
      unused_t, const Id<EventModel>& eventId, const TimeValue& newDate,
      double y, ExpandMode mode)
  {
    m_moveEventImplementation->update(eventId, newDate, y, mode);
    m_newY = y;
  }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  // TODO : make a UI to change that
  Path<Scenario::ProcessModel> m_scenario;
  Id<EventModel> m_eventId;
  double m_oldY{};
  double m_newY{};

  std::unique_ptr<SerializableMoveEvent> m_moveEventImplementation{};
};
}
}
