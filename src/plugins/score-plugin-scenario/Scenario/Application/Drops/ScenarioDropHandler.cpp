// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDropHandler.hpp"

#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <QFileInfo>
namespace Scenario
{

Scenario::StateModel*
closestLeftState(Scenario::Point pt,const Scenario::ScenarioPresenter& pres)
{
  const auto& scenario = pres.model();
  EventModel& start_ev = scenario.startEvent();
  SCORE_ASSERT(!start_ev.states().empty());
  Scenario::StateModel* cur_st = &scenario.states.at(start_ev.states().front());

  double cur_distance = std::numeric_limits<double>::max();
  for(auto& state : scenario.states)
  {
    const double rel_distance = std::abs(state.heightPercentage() - pt.y);
    //const double abs_x_distance = std::abs(Scenario::parentEvent(state, scenario).date().msec() / pres.zoomRatio() - pt.date.msec() / pres.zoomRatio());
    const double abs_y_distance = rel_distance * pres.view().height();

    if(rel_distance < cur_distance && (abs_y_distance < 15./* || abs_x_distance < 15. */))
    {
      auto& ev = scenario.event(state.eventId());
      if(ev.date() < pt.date)
      {
        cur_st = &state;
        cur_distance = rel_distance;
      }
    }
  }
  return cur_st;

  /*
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
  */
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

DropHandler::~DropHandler() {}
GhostIntervalDropHandler::~GhostIntervalDropHandler() {}

bool GhostIntervalDropHandler::dragEnter(
    const Scenario::ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  return dragMove(pres, pos, mime);
}

bool GhostIntervalDropHandler::dragMove(
    const Scenario::ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  bool mimeTypes = ossia::any_of(m_acceptableMimeTypes, [&] (const auto& mimeType){
    return mime.formats().contains(mimeType);
  });

  bool suffixes = false;
  for(auto& url : mime.urls())
  {
    if(url.isLocalFile())
    {
      const auto ext = QFileInfo{url.toLocalFile()}.suffix();
      suffixes |= ossia::any_of(m_acceptableSuffixes, [&] (const auto& suffix) {
        return ext == suffix;
      });
      if(suffixes)
        break;
    }
  }
  if (!(mimeTypes || suffixes))
    return false;

  auto pt = pres.toScenarioPoint(pos);
  auto st = closestLeftState(pt, pres);
  if (st)
  {
    if (st->nextInterval() || st->eventId() == pres.model().startEvent().id())
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

bool GhostIntervalDropHandler::dragLeave(
    const Scenario::ScenarioPresenter& pres,
    QPointF pos,
    const QMimeData& mime)
{
  pres.stopDrawDragLine();
  return false;
}

DropHandlerList::~DropHandlerList() {}

bool DropHandlerList::dragEnter(
    const ScenarioPresenter& scen,
    QPointF drop,
    const QMimeData& mime) const
{
  for (auto& fact : *this)
  {
    if (fact.dragEnter(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::dragMove(
    const ScenarioPresenter& scen,
    QPointF drop,
    const QMimeData& mime) const
{
  for (auto& fact : *this)
  {
    if (fact.dragMove(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::dragLeave(
    const ScenarioPresenter& scen,
    QPointF drop,
    const QMimeData& mime) const
{
  for (auto& fact : *this)
  {
    if (fact.dragLeave(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::drop(
    const ScenarioPresenter& scen,
    QPointF drop,
    const QMimeData& mime) const
{
  for (auto& fact : *this)
  {
    if (fact.drop(scen, drop, mime))
      return true;
  }

  return false;
}

IntervalDropHandler::~IntervalDropHandler() {}

IntervalDropHandlerList::~IntervalDropHandlerList() {}

bool IntervalDropHandlerList::drop(
    const score::DocumentContext& ctx,
    const Scenario::IntervalModel& cst,
    const QMimeData& mime) const
{
  for (auto& fact : *this)
  {
    if (fact.drop(ctx, cst, mime))
      return true;
  }

  return false;
}
}
