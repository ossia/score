#pragma once
#include <State/Expression.hpp>

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>

#include <score/command/AggregateCommand.hpp>
#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Scenario
{
class EventModel;
namespace Command
{
using EventModel = Scenario::EventModel;

class SCORE_PLUGIN_SCENARIO_EXPORT SetCondition final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetCondition, "Set an Event's condition")
public:
  SetCondition(const EventModel& event, State::Expression&& condition);
  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<EventModel> m_path;
  State::Expression m_condition;
  State::Expression m_previousCondition;
};

class SetEventScriptableMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(), SetEventScriptableMacro, "Set condition scriptable")
};

//! Flagging an event without a condition sets its condition to the published
//! boolean; unflagging removes that condition.
SCORE_PLUGIN_SCENARIO_EXPORT void setEventScriptable(
    const EventModel& event, bool scriptable, const score::DocumentContext& ctx);

class SetOffsetBehavior final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetOffsetBehavior, "Set offset behavior")
public:
  SetOffsetBehavior(const EventModel& event, OffsetBehavior newval);
};
}
}

PROPERTY_COMMAND_T(
    Scenario::Command, SetEventScriptable, EventModel::p_scriptable,
    "Set condition scriptable")
SCORE_COMMAND_DECL_T(Scenario::Command::SetEventScriptable)
