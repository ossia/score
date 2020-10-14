// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioGlobalCommandManager.hpp"

#include <Scenario/Commands/ClearSelection.hpp>
#include <Scenario/Commands/Interval/RemoveProcessFromInterval.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearInterval.hpp>
#include <Scenario/Commands/Scenario/Deletions/ClearState.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveMacro.hpp>
#include <Scenario/Commands/Scenario/Deletions/RemoveSelection.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeEvents.hpp>
#include <Scenario/Commands/Scenario/Merge/MergeTimeSyncs.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactory.hpp>
#include <Scenario/Commands/TimeSync/TriggerCommandFactory/TriggerCommandFactoryList.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/std/Optional.hpp>

#include <QSet>

namespace score
{
class CommandStackFacade;
} // namespace score

using namespace Scenario::Command;
using namespace score::IDocument; // for ::path

namespace Scenario
{
void clearContentFromSelection(
    const Scenario::ScenarioInterface& si,
    const score::DocumentContext& ctx)
{
  auto statesToRemove = selectedElements(si.getStates());
  auto intervalsToRemove = selectedElements(si.getIntervals());

  if (!statesToRemove.empty() && !intervalsToRemove.empty())
  {
    MacroCommandDispatcher<ClearSelection> cleaner{ctx.commandStack};

    // Create a Clear command for each.

    for (auto& state : statesToRemove)
    {
      if (state->messages().rootNode().hasChildren())
      {
        cleaner.submit(new ClearState(*state));
      }
    }

    for (auto& interval : intervalsToRemove)
    {
      cleaner.submit(new ClearInterval(*interval));
      // if a state and an interval are selected then remove event too
    }

    cleaner.commit();
    return;
  }

  auto syncsToRemove = selectedElements(si.getTimeSyncs());
  if (!syncsToRemove.empty())
  {
    MacroCommandDispatcher<ClearSelection> cleaner{ctx.commandStack};
    const auto& triggerCommands = ctx.app.interfaces<TriggerCommandFactoryList>();
    for (auto& sync : syncsToRemove)
    {
      if (sync->active())
      {
        auto cmd = triggerCommands.make(&TriggerCommandFactory::make_removeTriggerCommand, *sync);
        if (cmd)
        {
          cleaner.submit(std::move(cmd));
        }
      }
    }
    cleaner.commit();
    return;
  }
}

void removeSelection(const Scenario::ProcessModel& scenario, const score::DocumentContext& ctx)
{
  auto& stack = ctx.commandStack;
  RedoMacroCommandDispatcher<ClearSelection> cleaner{stack};

  const auto& states = selectedElements(scenario.getStates());

  // Create a Clear command for each.
  // TODO why ? except the first one, they are going to be removed anyways...

  for (auto& state : states)
  {
    if (state->messages().rootNode().hasChildren())
    {
      cleaner.submit(new ClearState(*state));
    }
  }

  Selection sel = scenario.selectedChildren();

  for (const auto& obj : ctx.selectionStack.currentSelection())
  {
    if (auto proc = dynamic_cast<const Process::ProcessModel*>(obj.data()))
    {
      bool remove_it = true;

      for (const auto& obj : sel)
      {
        if (auto itv = dynamic_cast<const IntervalModel*>(obj.data()))
        {
          if (itv == proc->parent())
          {
            remove_it = false;
            break;
          }
        }
      }

      if (remove_it)
      {
        // TODO the qobject_cast is needed because some processes have
        // non-interval parents (e.g in Nodal) How do we fix this generically ?
        if (auto itv = qobject_cast<IntervalModel*>(proc->parent()))
        {
          cleaner.submit(new Scenario::Command::RemoveProcessFromInterval(*itv, proc->id()));
        }
      }
    }
  }

  score::SelectionDispatcher s{ctx.selectionStack};
  s.deselect();
  ctx.selectionStack.clear();
  // We have to remove the first / last timesyncs / events from the selection.
  ossia::erase_if(sel, [&](auto&& elt) { return elt->id_val() == startId_val; });

  if (!sel.empty())
  {
    Command::setupRemoveMacro(scenario, sel, cleaner);
    // cleaner.submit(new RemoveSelection(scenario, sel));
  }

  cleaner.commit();
}

template <typename T>
auto make_ordered(const Scenario::ProcessModel& scenario)
{
  using comp_t = StartDateComparator<T>;
  using set_t = std::set<const T*, comp_t>;

  set_t the_set(comp_t{&scenario});

  auto cont = Scenario::ElementTraits<Scenario::ProcessModel, T>::accessor;
  for (auto& tn : selectedElements(cont(scenario)))
  {
    the_set.insert(tn);
  }
  return the_set;
}

void mergeTimeSyncs(const Scenario::ProcessModel& scenario, const score::CommandStackFacade& f)
{
  // We merge all the furthest timesyncs to the first one.
  auto timesyncs = make_ordered<TimeSyncModel>(scenario);
  auto states = make_ordered<StateModel>(scenario);

  if (timesyncs.size() < 2)
  {
    if (states.size() == 2)
    {
      auto it = states.begin();
      auto& first = Scenario::parentTimeSync(**it, scenario);
      auto& second = Scenario::parentTimeSync(**(++it), scenario);

      auto cmd = new Command::MergeTimeSyncs(scenario, second.id(), first.id());
      f.redoAndPush(cmd);
    }
  }
  else
  {
    auto it = timesyncs.begin();
    auto first_tn = (*it)->id();
    for (++it; it != timesyncs.end(); ++it)
    {
      auto cmd = new Command::MergeTimeSyncs(scenario, first_tn, (*it)->id());
      f.redoAndPush(cmd);
    }
  }
}

void mergeEvents(const Scenario::ProcessModel& scenario, const score::CommandStackFacade& f)
{
  // We merge all the furthest events to the first one.
  const auto& states = selectedElements(scenario.getStates());
  const auto& events = selectedElements(scenario.getEvents());

  auto sel = events;

  for (auto it = states.begin(); it != states.end(); ++it)
  {
    sel.push_back(&Scenario::parentEvent(**it, scenario));
  }

  ossia::remove_duplicates(sel);

  MacroCommandDispatcher<MergeEventMacro> merger{f};

  while (sel.size() > 1)
  {
    auto it = sel.begin();
    auto first_ev = *it;
    for (++it; it != sel.end();)
    {
      // Check if Events have the same TimeSync parent
      // if (first_ev->timeSync() == (*it)->timeSync())
      {
        auto cmd = new Command::MergeEvents(scenario, first_ev->id(), (*it)->id());
        // auto cmd = new Command::MergeTimeSyncs(scenario,
        // first_ev->timeSync(), (*it)->timeSync());
        // f.redoAndPush(cmd);
        merger.submit(cmd);
        it = sel.erase(it);
      }
      // else
      //  ++it;
    }
    ossia::remove_one(sel, first_ev);
  }
  merger.commit();
}

bool EndDateComparator::operator()(const IntervalModel* lhs, const IntervalModel* rhs) const
{
  return (Scenario::date(*lhs, *scenario) + lhs->duration.defaultDuration())
         < (Scenario::date(*rhs, *scenario) + rhs->duration.defaultDuration());
}
}
