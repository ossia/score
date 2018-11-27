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
void AddProcess(IntervalModel& interval, Process::ProcessModel* proc)
{
  interval.processes.add(proc);
}

void RemoveProcess(
    IntervalModel& interval, const Id<Process::ProcessModel>& proc_id)
{
  auto& proc = interval.processes.at(proc_id);
  interval.processes.remove(proc);
}

void EraseProcess(
    IntervalModel& interval, const Id<Process::ProcessModel>& proc_id)
{
  auto& proc = interval.processes.at(proc_id);
  interval.processes.erase(proc);
}

void SetPreviousInterval(StateModel& state, const IntervalModel& interval)
{
  SetNoPreviousInterval(state);

  state.setPreviousInterval(interval.id());
}

void SetNextInterval(StateModel& state, const IntervalModel& interval)
{
  SetNoNextInterval(state);

  state.setNextInterval(interval.id());
}

void SetNoPreviousInterval(StateModel& state)
{
  if (state.previousInterval())
  {
    auto node = state.messages().rootNode();
    state.messages() = std::move(node);

    state.setPreviousInterval(OptionalId<IntervalModel>{});
  }
}

void SetNoNextInterval(StateModel& state)
{
  if (state.nextInterval())
  {
    auto node = state.messages().rootNode();
    state.messages() = std::move(node);

    state.setNextInterval(OptionalId<IntervalModel>{});
  }
}
}
