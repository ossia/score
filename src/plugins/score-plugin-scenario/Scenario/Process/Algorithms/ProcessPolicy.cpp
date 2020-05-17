// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessPolicy.hpp"

#include <Process/State/ProcessStateDataInterface.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <ossia/detail/algorithms.hpp>
namespace Scenario
{
static void AddProcessBeforeState(StateModel& statemodel, const Process::ProcessModel& proc)
{
  // TODO this should be fused with the notion of State Process.
  ProcessStateDataInterface* state = proc.endStateData();
  if (!state)
    return;

  auto prev_proc_fun = [&](const State::MessageList& ml) {
    // TODO have some collapsing between all the processes of a state
    // NOTE how to prevent these states from being played
    // twice ? mark them ?
    // TODO which one should be sent ? the ones
    // from the process ?
    auto& messages = statemodel.messages();

    for (const ProcessStateWrapper& next_proc : statemodel.followingProcesses())
    {
      next_proc.process().setMessages(ml, messages.rootNode());
    }

    updateTreeWithMessageList(messages.rootNode(), ml, proc.id(), ProcessPosition::Previous);
    statemodel.sig_statesUpdated();
  };

  statemodel.previousProcesses().emplace_back(state);
  auto& wrapper = statemodel.previousProcesses().back();
  QObject::connect(state, &ProcessStateDataInterface::messagesChanged, &wrapper, prev_proc_fun);

  prev_proc_fun(state->messages());
}

static void AddProcessAfterState(StateModel& statemodel, const Process::ProcessModel& proc)
{
  ProcessStateDataInterface* state = proc.startStateData();
  if (!state)
    return;

  auto next_proc_fun = [&](const State::MessageList& ml) {
    auto& messages = statemodel.messages();

    for (const ProcessStateWrapper& prev_proc : statemodel.previousProcesses())
    {
      prev_proc.process().setMessages(ml, messages.rootNode());
    }

    updateTreeWithMessageList(messages.rootNode(), ml, proc.id(), ProcessPosition::Following);
    statemodel.sig_statesUpdated();
  };

  statemodel.followingProcesses().emplace_back(state);
  auto& wrapper = statemodel.followingProcesses().back();
  QObject::connect(state, &ProcessStateDataInterface::messagesChanged, &wrapper, next_proc_fun);

  next_proc_fun(state->messages());
}

static void RemoveProcessBeforeState(StateModel& statemodel, const Process::ProcessModel& proc)
{
  ProcessStateDataInterface* state = proc.endStateData();
  if (!state)
    return;

  auto it = ossia::find_if(
      statemodel.previousProcesses(), [&](const auto& elt) { return state == &elt.process(); });

  updateTreeWithRemovedProcess(
      statemodel.messages().rootNode(), proc.id(), ProcessPosition::Previous);
  statemodel.sig_statesUpdated();

  // TODO debug the need for this check
  if (it != statemodel.previousProcesses().end())
    statemodel.previousProcesses().erase(it);
}

static void RemoveProcessAfterState(StateModel& statemodel, const Process::ProcessModel& proc)
{
  ProcessStateDataInterface* state = proc.startStateData();
  if (!state)
    return;

  auto it = ossia::find_if(
      statemodel.followingProcesses(), [&](const auto& elt) { return state == &elt.process(); });

  updateTreeWithRemovedProcess(
      statemodel.messages().rootNode(), proc.id(), ProcessPosition::Following);
  statemodel.sig_statesUpdated();

  // TODO debug the need for this check
  if (it != statemodel.followingProcesses().end())
    statemodel.followingProcesses().erase(it);
}

void AddProcess(IntervalModel& interval, Process::ProcessModel* proc)
{
  interval.processes.add(proc);

  const auto& scenar = *dynamic_cast<ScenarioInterface*>(interval.parent());
  AddProcessAfterState(startState(interval, scenar), *proc);
  AddProcessBeforeState(endState(interval, scenar), *proc);
}

void RemoveProcess(IntervalModel& interval, const Id<Process::ProcessModel>& proc_id)
{
  auto& proc = interval.processes.at(proc_id);
  const auto& scenar = *dynamic_cast<ScenarioInterface*>(interval.parent());

  RemoveProcessAfterState(startState(interval, scenar), proc);
  RemoveProcessBeforeState(endState(interval, scenar), proc);

  interval.processes.remove(proc);
}

void EraseProcess(IntervalModel& interval, const Id<Process::ProcessModel>& proc_id)
{
  auto& proc = interval.processes.at(proc_id);
  const auto& scenar = *dynamic_cast<ScenarioInterface*>(interval.parent());

  RemoveProcessAfterState(startState(interval, scenar), proc);
  RemoveProcessBeforeState(endState(interval, scenar), proc);

  interval.processes.erase(proc);
}

void SetPreviousInterval(StateModel& state, const IntervalModel& interval)
{
  SetNoPreviousInterval(state);

  state.setPreviousInterval(interval.id());
  for (const auto& proc : interval.processes)
  {
    AddProcessBeforeState(state, proc);
  }
}

void SetNextInterval(StateModel& state, const IntervalModel& interval)
{
  SetNoNextInterval(state);

  state.setNextInterval(interval.id());
  for (const auto& proc : interval.processes)
  {
    AddProcessAfterState(state, proc);
  }
}

void SetNoPreviousInterval(StateModel& state)
{
  if (state.previousInterval())
  {
    auto node = state.messages().rootNode();
    updateTreeWithRemovedInterval(node, ProcessPosition::Previous);
    state.messages() = std::move(node);

    state.previousProcesses().clear();
    state.setPreviousInterval(OptionalId<IntervalModel>{});
  }
}

void SetNoNextInterval(StateModel& state)
{
  if (state.nextInterval())
  {
    auto node = state.messages().rootNode();
    updateTreeWithRemovedInterval(node, ProcessPosition::Following);
    state.messages() = std::move(node);

    state.followingProcesses().clear();
    state.setNextInterval(OptionalId<IntervalModel>{});
  }
}
}
