// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessageDropHandler.hpp"

#include <QMimeData>
#include <Scenario/Application/ScenarioValidity.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateStateMacro.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeSync_Event_State.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ContainersAccessors.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <State/MessageListSerialization.hpp>
#include <QFile>
#include <QFileInfo>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

namespace Scenario
{
static Scenario::StateModel*
closestLeftState(Scenario::Point pt, const Scenario::ProcessModel& scenario)
{
  TimeSyncModel* cur_tn = &scenario.startTimeSync();
  for (auto& tn : scenario.timeSyncs)
  {
    auto date = tn.date();
    if (date > cur_tn->date() && date < pt.date)
    {
      cur_tn = &tn;
    }
  }

  auto states = Scenario::states(*cur_tn, scenario);
  if (!states.empty())
  {
    auto cur_st = &scenario.states.at(states.front());
    for (auto state_id : states)
    {
      auto& state = scenario.states.at(state_id);
      if (std::abs(state.heightPercentage() - pt.y)
          < std::abs(cur_st->heightPercentage() - pt.y))
      {
        cur_st = &state;
      }
    }
    return cur_st;
  }
  return nullptr;
}

/*
static Scenario::StateModel* magneticLeftState(Scenario::Point pt, const
Scenario::ProcessModel& scenario)
{
  Scenario::StateModel* cur_st = &*scenario.states.begin();

  for(auto& state : scenario.states)
  {
      if(std::abs(state.heightPercentage() - pt.y) <
std::abs(cur_st->heightPercentage() - pt.y))
      {
        auto& new_ev = scenario.event(state.eventId());
        if(new_ev.date() < pt.date)
          cur_st = &state;
      }
  }
  return cur_st;
}
*/

bool MessageDropHandler::dragEnter(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  return dragMove(pres, pos, mime);
}

bool MessageDropHandler::dragMove(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  if (!mime.formats().contains(score::mime::messagelist()) && !mime.hasUrls())
    return false;

  auto pt = pres.toScenarioPoint(pos);
  auto st = closestLeftState(pt, pres.model());
  if (st)
  {
    if (st->nextInterval())
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
    const QMimeData& mime)
{
  pres.stopDrawDragLine();
  return false;
}

bool MessageDropHandler::drop(
    const Scenario::TemporalScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  using namespace Scenario::Command;
  pres.stopDrawDragLine();

  // If the mime data has states in it we can handle it.
  State::MessageList ml;
  if (mime.formats().contains(score::mime::messagelist()))
  {
    Mime<State::MessageList>::Deserializer des{mime};
    ml = des.deserialize();
  }
  else if(mime.hasUrls())
  {
    for(const auto& u : mime.urls())
    {
      auto path = u.toLocalFile();
      if(QFile f{path}; QFileInfo{f}.suffix() == "cues" && f.open(QIODevice::ReadOnly))
      {
        State::MessageList sub;
        fromJsonArray(
            QJsonDocument::fromJson(f.readAll()).array(),
            sub);
        ml += sub;
      }
    }
  }

  if(ml.empty())
    return true;

  Scenario::Command::Macro m{
    new Scenario::Command::CreateStateMacro,
    pres.context().context};

  const Scenario::ProcessModel& scenar = pres.model();
  Id<StateModel> createdState;

  Scenario::Point pt = pres.toScenarioPoint(pos);

  auto state = closestLeftState(pt, scenar);
  if (state)
  {
    if (state->nextInterval())
    {
      // We create from the event instead
      auto& s = m.createState(scenar, state->eventId(), pt.y);
      auto& i = m.createIntervalAfter(scenar, s.id(), pt);
      createdState = i.endState();
    }
    else
    {
      auto& i = m.createIntervalAfter(scenar, state->id(), {pt.date, state->heightPercentage()});
      createdState = i.endState();
    }
  }
  else
  {
    // We create in the emptiness
    const auto& [t, e, s] = m.createDot(scenar, pt);
    createdState = s.id();
  }

  m.addMessages(scenar.state(createdState), std::move(ml));

  m.commit();
  return true;
}
}
