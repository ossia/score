#pragma once
#include <Process/State/MessageNode.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <State/Message.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>

#include <QMap>

#include <score_plugin_scenario_export.h>
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
class SCORE_PLUGIN_SCENARIO_EXPORT AddMessagesToState final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), AddMessagesToState,
      "Add messages to state")
public:
  AddMessagesToState(
      const Scenario::StateModel& state, const State::MessageList& messages);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<StateModel> m_path;

  State::MessageList m_oldState, m_newState;
};

class SCORE_PLUGIN_SCENARIO_EXPORT ReplaceMessagesInState final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(), ReplaceMessagesInState,
      "Replace messages in a state")
public:
  ReplaceMessagesInState(
      const Scenario::StateModel& state, State::MessageList&& messages);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<StateModel> m_path;

  State::MessageList m_oldState, m_newState;
};
}
}
