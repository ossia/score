#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class ProcessModel;
class EventModel;
class StateModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT CreateState final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), CreateState, "Create a state")
public:
  CreateState(
      const Scenario::ProcessModel& scenario,
      Id<EventModel>
          event,
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

  void undo(const iscore::DocumentContext& ctx) const override;

  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Scenario::ProcessModel> m_path;

  Id<StateModel> m_newState;
  Id<EventModel> m_event;
  double m_stateY{};
};
}
}
