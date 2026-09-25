#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <State/Message.hpp>

#include <score/command/AggregateCommand.hpp>

#include <score_plugin_scenario_export.h>

#include <vector>

namespace Process
{
class ProcessModel;
class ControlInlet;
}
namespace score
{
struct DocumentContext;
}
namespace Scenario
{
class StateModel;
namespace Command
{
class SnapshotInState final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), SnapshotInState, "Snapshot in state")
};

//! One message per control plus one for the internal state, if any;
//! unpublished controls and processes get flagged scriptable.
SCORE_PLUGIN_SCENARIO_EXPORT void snapshotProcessInState(
    const StateModel& state, const Process::ProcessModel& process,
    const score::DocumentContext& ctx);

class Macro;

//! withState adds the state beyond the controls
SCORE_PLUGIN_SCENARIO_EXPORT State::MessageList snapshotMessages(
    const std::vector<const Process::ProcessModel*>& processes, Macro& m,
    bool withState = true);

SCORE_PLUGIN_SCENARIO_EXPORT void snapshotProcessesInState(
    const StateModel& state, const std::vector<const Process::ProcessModel*>& processes,
    const score::DocumentContext& ctx, bool withState = true);

SCORE_PLUGIN_SCENARIO_EXPORT void snapshotControlInState(
    const StateModel& state, const Process::ControlInlet& control,
    const score::DocumentContext& ctx);
}
}
