// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioGlobalCommandManager.hpp"
#include <QDebug>
#include <Scenario/Commands/ClearSelection.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearInterval.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearState.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveSelection.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeTimeSyncs.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>

#include <algorithm>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/tools/std/Optional.hpp>

namespace score
{
class CommandStackFacade;
} // namespace score

using namespace Scenario::Command;
using namespace score::IDocument; // for ::path

namespace Scenario
{
bool clearContentFromSelection(
    const QList<const IntervalModel*>& intervalsToRemove,
    const QList<const StateModel*>& statesToRemove,
    const score::CommandStackFacade& stack)
{
  MacroCommandDispatcher<ClearSelection> cleaner{stack};

  bool flag = false;

  // Create a Clear command for each.

  for (auto& state : statesToRemove)
  {
    if (state->messages().rootNode().hasChildren() )
    {
      cleaner.submitCommand(new ClearState(*state));
      flag = true;
    }
  }

  for (auto& interval : intervalsToRemove)
  {
    cleaner.submitCommand(new ClearInterval(*interval));
    // if a state and an interval are selected then remove event too
    flag = false;
  }

  cleaner.commit();

  return flag;
}

bool clearContentFromSelection(
    const Scenario::ProcessModel& scenario,
    const score::CommandStackFacade& stack)
{
  return clearContentFromSelection(
      selectedElements(scenario.intervals),
      selectedElements(scenario.states),
      stack);
}

template <typename Range, typename Fun>
void erase_if(Range& r, Fun f)
{
  for (auto it = std::begin(r); it != std::end(r);)
  {
    it = f(*it) ? r.erase(it) : ++it;
  }
}

void removeSelection(
    const Scenario::ProcessModel& scenario,
    const score::CommandStackFacade& stack)
{
  MacroCommandDispatcher<ClearSelection> cleaner{stack};

  const QList<const IntervalModel*>& intervals = selectedElements(scenario.getIntervals());
  const QList<const StateModel*>& states = selectedElements(scenario.getStates());

  // Create a Clear command for each.

  for (auto& state : states)
  {
    if (state->messages().rootNode().hasChildren() )
    {
      cleaner.submitCommand(new ClearState(*state));
    }
  }

  for (auto& interval : intervals)
  {
    cleaner.submitCommand(new ClearInterval(*interval));
  }

  Selection sel = scenario.selectedChildren();

  auto& ctx = score::IDocument::documentContext(scenario);
  score::SelectionDispatcher s{ctx.selectionStack};
  s.setAndCommit({});
  ctx.selectionStack.clear();
  // We have to remove the first / last timesyncs / events from the selection.
  erase_if(sel, [&](auto&& elt) { return elt->id_val() == startId_val(); });

  if (!sel.empty())
  {
    cleaner.submitCommand(new RemoveSelection(scenario, sel));
  }

  cleaner.commit();
}

void removeSelection(
    const BaseScenario&, const score::CommandStackFacade&)
{
  // Shall do nothing
}

bool clearContentFromSelection(
    const BaseScenarioContainer& scenario,
    const score::CommandStackFacade& stack)
{
  QList<const Scenario::IntervalModel*> itv;
  QList<const Scenario::StateModel*> states;
  if (scenario.interval().selection.get())
    itv.push_back(&scenario.interval());
  if (scenario.startState().selection.get())
    states.push_back(&scenario.startState());
  if (scenario.endState().selection.get())
    states.push_back(&scenario.endState());

  return clearContentFromSelection(itv, states, stack);
}

bool clearContentFromSelection(
    const BaseScenario& scenario, const score::CommandStackFacade& stack)
{
  return clearContentFromSelection(
      static_cast<const BaseScenarioContainer&>(scenario), stack);
}

bool clearContentFromSelection(
    const Scenario::ScenarioInterface& scenario,
    const score::CommandStackFacade& stack)
{
  return clearContentFromSelection(
      selectedElements(scenario.getIntervals()),
      selectedElements(scenario.getStates()),
      stack);
}

// MOVEME : these are useful.
template <typename T>
struct DateComparator
{
  const Scenario::ProcessModel* scenario;
  bool operator()(const T* lhs, const T* rhs)
  {
    return Scenario::date(*lhs, *scenario) < Scenario::date(*rhs, *scenario);
  }
};

template <typename T>
auto make_ordered(const Scenario::ProcessModel& scenario)
{
  using comp_t = DateComparator<T>;
  using set_t = std::set<const T*, comp_t>;

  set_t the_set(comp_t{&scenario});

  auto cont
      = Scenario::ElementTraits<Scenario::ProcessModel, T>::accessor;
  for (auto& tn : selectedElements(cont(scenario)))
  {
    the_set.insert(tn);
  }
  return the_set;
}

void mergeTimeSyncs(
    const Scenario::ProcessModel& scenario,
    const score::CommandStackFacade& f)
{
  // We merge all the furthest timesyncs to the first one.
  auto timesyncs = make_ordered<TimeSyncModel>(scenario);
  auto states = make_ordered<StateModel>(scenario);
  auto events = make_ordered<EventModel>(scenario);

  if (timesyncs.size() < 2)
  {
    if (states.size() == 2)
    {
      auto it = states.begin();
      auto& first = Scenario::parentTimeSync(**it, scenario);
      auto& second = Scenario::parentTimeSync(**(++it), scenario);

      auto cmd = new Command::MergeTimeSyncs(
          scenario, second.id(), first.id());
      f.redoAndPush(cmd);
    }
  }
  else
  {
    auto it = timesyncs.begin();
    auto first_tn = (*it)->id();
    for (++it; it != timesyncs.end(); ++it)
    {
      auto cmd = new Command::MergeTimeSyncs(
          scenario, first_tn, (*it)->id());
      f.redoAndPush(cmd);
    }
  }
}
}
