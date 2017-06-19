#pragma once
#include <QMap>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

#include <Process/State/MessageNode.hpp>
#include <State/Message.hpp>
#include <iscore/model/Identifier.hpp>

#include <iscore_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModel;
}

namespace Scenario
{
class StateModel;
namespace Command
{
class ISCORE_PLUGIN_SCENARIO_EXPORT AddMessagesToState final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      AddMessagesToState,
      "Add messages to state")
public:
  AddMessagesToState(
      const Scenario::StateModel& state,
      const State::MessageList& messages);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<StateModel> m_path;

  Process::MessageNode m_oldState, m_newState;

  QMap<Id<Process::ProcessModel>, State::MessageList> m_previousBackup;
  QMap<Id<Process::ProcessModel>, State::MessageList> m_followingBackup;
};
}
}
