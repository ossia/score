#pragma once
#include <QString>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>

#include <score/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ProcessModel;
class EventModel;
class StateModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT CreateState final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateState, "Create a state")
public:
  CreateState(
      const Scenario::ProcessModel& scenario,
      Id<EventModel> event,
      double stateY);
  CreateState(
      const Scenario::ProcessModel& scenario,
      Id<StateModel> newId,
      Id<EventModel> event,
      double stateY);

  const Path<Scenario::ProcessModel>& scenarioPath() const
  {
    return m_path;
  }

  const double& endStateY() const
  {
    return m_stateY;
  }

  const Id<StateModel>& createdState() const
  {
    return m_newState;
  }

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;
  QString m_createdName;
  Id<StateModel> m_newState;
  Id<EventModel> m_event;
  double m_stateY{};
};
}
}
