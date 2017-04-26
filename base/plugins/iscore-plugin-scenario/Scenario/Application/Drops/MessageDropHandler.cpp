#include "MessageDropHandler.hpp"

#include <Scenario/Application/ScenarioValidity.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateStateMacro.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <State/MessageListSerialization.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <QMimeData>

namespace Scenario
{
static Scenario::StateModel* closestLeftState(Scenario::Point pt, const Scenario::ProcessModel& scenario)
{
  TimeNodeModel* cur_tn = &scenario.startTimeNode();
  for(auto& tn : scenario.timeNodes)
  {
    auto date = tn.date();
    if(date > cur_tn->date() && date < pt.date)
    {
      cur_tn = &tn;
    }
  }

  auto states = Scenario::states(*cur_tn, scenario);
  if(!states.empty())
  {
    auto cur_st = &scenario.states.at(states.front());
    for(auto state_id : states)
    {
      auto& state = scenario.states.at(state_id);
      if(std::abs(state.heightPercentage() - pt.y) < std::abs(cur_st->heightPercentage() - pt.y))
      {
        cur_st = &state;
      }
    }
    return cur_st;
  }
  return nullptr;
}


static Scenario::StateModel* magneticLeftState(Scenario::Point pt, const Scenario::ProcessModel& scenario)
{
  Scenario::StateModel* cur_st = &*scenario.states.begin();

  for(auto& state : scenario.states)
  {
      if(std::abs(state.heightPercentage() - pt.y) < std::abs(cur_st->heightPercentage() - pt.y))
      {
        auto& new_ev = scenario.event(state.eventId());
        if(new_ev.date() < pt.date)
          cur_st = &state;
      }
  }
  return cur_st;
}


bool MessageDropHandler::dragEnter(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData* mime)
{
  return dragMove(pres, pos, mime);
}

bool MessageDropHandler::dragMove(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData* mime)
{
  if (!mime->formats().contains(iscore::mime::messagelist()))
    return false;

  auto pt = pres.toScenarioPoint(pos);
  auto st = closestLeftState(pt, pres.processModel());
  if(st)
  {
    if (st->nextConstraint())
    {
      pres.drawDragLine(*st, pt);
    }
    else
    {
      // Sequence
      pres.drawDragLine(*st, {pt.date, st->heightPercentage()});
    }
  }
  return true;
}

bool MessageDropHandler::dragLeave(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData* mime)
{
  pres.stopDrawDragLine();
  return false;
}

bool MessageDropHandler::drop(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData* mime)
{
  using namespace Scenario::Command;
  // If the mime data has states in it we can handle it.
  if (!mime->formats().contains(iscore::mime::messagelist()))
    return false;

  pres.stopDrawDragLine();

  Mime<State::MessageList>::Deserializer des{*mime};
  State::MessageList ml = des.deserialize();

  RedoMacroCommandDispatcher<Scenario::Command::CreateStateMacro> m{
      pres.context().context.commandStack};

  const Scenario::ProcessModel& scenar = pres.processModel();
  Id<StateModel> createdState;

  Scenario::Point pt = pres.toScenarioPoint(pos);

  auto state = closestLeftState(pt, scenar);
  if (state)
  {
    if (state->nextConstraint())
    {
      // We create from the event instead
      auto cmd1
          = new Scenario::Command::CreateState{scenar, state->eventId(), pt.y};
      m.submitCommand(cmd1);

      auto cmd2 = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
          scenar, cmd1->createdState(), pt.date, pt.y};
      m.submitCommand(cmd2);
      createdState = cmd2->createdState();
    }
    else
    {
      auto cmd = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
          scenar, state->id(), pt.date, state->heightPercentage()};
      m.submitCommand(cmd);
      createdState = cmd->createdState();
    }
  }
  else
  {
    // We create in the emptiness
    auto cmd = new Scenario::Command::CreateTimeNode_Event_State(
        scenar, pt.date, pt.y);
    m.submitCommand(cmd);
    createdState = cmd->createdState();
  }

  auto state_path = make_path(scenar).extend(createdState);

  auto cmd2 = new AddMessagesToState{std::move(state_path), ml};

  m.submitCommand(cmd2);

  m.commit();
  return true;
}
}
