#include "SnapshotProcess.hpp"

#include <Process/Commands/EditPort.hpp>
#include <Process/Commands/Properties.hpp>
#include <Process/Preset.hpp>
#include <Process/PresetHelpers.hpp>
#include <Process/ProcessState.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/State/StateModel.hpp>

#include <LocalTree/ScriptableProcessComponent.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

namespace Scenario::Command
{
namespace
{
template <typename Dispatcher>
void addControl(
    State::MessageList& messages, const Process::ControlInlet& control,
    Dispatcher& disp)
{
  if(!LocalTree::wantsPublished(control))
    disp.submit(new Process::SetPortScriptable{control, true});
  if(auto addr = LocalTree::scriptableAddress(control); addr.isSet())
    messages.push_back({State::AddressAccessor{addr}, control.value()});
}
}

template <typename Dispatcher>
static void addProcess(
    State::MessageList& messages, const Process::ProcessModel& process,
    Dispatcher& disp, bool withState)
{
  // Flagging the process publishes all its controls at once
  const auto state = withState ? Process::stateBeyondControls(process) : QByteArray{};
  bool controls = false;
  process.forEachControl(
      [&](Process::ControlInlet&, const ossia::value&) { controls = true; });
  if((controls || !state.isEmpty()) && !process.scriptable())
    disp.submit(new Process::SetProcessScriptable{process, true});
  process.forEachControl([&](Process::ControlInlet& control, const ossia::value&) {
    addControl(messages, control, disp);
  });
  if(!state.isEmpty())
  {
    if(auto addr = LocalTree::scriptableStateAddress(process); addr.isSet())
      messages.push_back({State::AddressAccessor{addr}, state.toStdString()});
  }
}

void snapshotProcessInState(
    const StateModel& state, const Process::ProcessModel& process,
    const score::DocumentContext& ctx)
{
  snapshotProcessesInState(state, {&process}, ctx);
}

State::MessageList snapshotMessages(
    const std::vector<const Process::ProcessModel*>& processes, Macro& m, bool withState)
{
  State::MessageList messages;
  for(auto process : processes)
    addProcess(messages, *process, m, withState);
  return messages;
}

void snapshotProcessesInState(
    const StateModel& state, const std::vector<const Process::ProcessModel*>& processes,
    const score::DocumentContext& ctx, bool withState)
{
  RedoMacroCommandDispatcher<SnapshotInState> disp{ctx.commandStack};
  State::MessageList messages;
  for(auto process : processes)
    addProcess(messages, *process, disp, withState);

  if(messages.empty())
    return disp.rollback();
  disp.submit(new AddMessagesToState{state, messages});
  disp.commit();
}

void snapshotControlInState(
    const StateModel& state, const Process::ControlInlet& control,
    const score::DocumentContext& ctx)
{
  RedoMacroCommandDispatcher<SnapshotInState> disp{ctx.commandStack};
  State::MessageList messages;
  addControl(messages, control, disp);
  if(messages.empty())
    return disp.rollback();
  disp.submit(new AddMessagesToState{state, messages});
  disp.commit();
}
}
