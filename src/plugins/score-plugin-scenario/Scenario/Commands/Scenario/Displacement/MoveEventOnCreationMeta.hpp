#pragma once

#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/Scenario/Displacement/SerializableMoveEvent.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

#include <score_plugin_scenario_export.h>

#include <memory>
struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
class ProcessModel;
namespace Command
{

class SCORE_PLUGIN_SCENARIO_EXPORT MoveEventOnCreationMeta final : public SerializableMoveEvent
{
  SCORE_COMMAND_DECL(CommandFactoryName(), MoveEventOnCreationMeta, "Move an event on creation")
public:
  MoveEventOnCreationMeta(
      const Scenario::ProcessModel& scenarioPath,
      Id<EventModel> eventId,
      TimeVal newDate,
      ExpandMode mode);
  ~MoveEventOnCreationMeta();

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Path<Scenario::ProcessModel>& path() const override;
  void update(
      Scenario::ProcessModel& scenario,
      const Id<EventModel>& eventId,
      const TimeVal& newDate,
      double,
      ExpandMode mode,
      LockMode lm) override;

  // Command interface
protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  std::unique_ptr<SerializableMoveEvent> m_moveEventImplementation;
};
}
}
