#pragma once
#include <Process/ControlMessage.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Message.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

#include <QMap>

#include <score_plugin_scenario_export.h>
namespace Process
{
class ProcessModel;
struct ControlMessage;
}

namespace Scenario
{
class StateModel;
namespace Command
{
class SCORE_PLUGIN_SCENARIO_EXPORT AddMessagesToState final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), AddMessagesToState, "Add messages to state")
public:
  AddMessagesToState(const Scenario::StateModel& state, const State::MessageList& messages);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<StateModel> m_path;

  Process::MessageNode m_oldState, m_newState;

  QMap<Id<Process::ProcessModel>, State::MessageList> m_previousBackup;
  QMap<Id<Process::ProcessModel>, State::MessageList> m_followingBackup;
};


class RenameAddressInState final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RenameAddressInState, "Rename address in a state")

public:
  RenameAddressInState(
      const Scenario::StateModel& state,
      const State::AddressAccessor& old,
      const State::AddressAccessorHead& name);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

private:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

  Path<StateModel> m_state;
  State::AddressAccessor m_oldName, m_newName;
};

class SCORE_PLUGIN_SCENARIO_EXPORT AddControlMessagesToState final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      AddControlMessagesToState,
      "Add control messages to state")
public:
  AddControlMessagesToState(
      const Scenario::StateModel& state,
      std::vector<Process::ControlMessage>&& messages);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

private:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;
  Path<StateModel> m_path;

  std::vector<Process::ControlMessage> m_old, m_new;
};
}
}
