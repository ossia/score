#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <State/Expression.hpp>

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

class SetOffsetBehavior final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SetOffsetBehavior, "Set offset behavior")
public:
  SetOffsetBehavior(const EventModel& event, OffsetBehavior newval);
};
}
}
