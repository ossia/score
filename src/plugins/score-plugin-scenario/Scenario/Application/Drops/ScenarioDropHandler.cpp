// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDropHandler.hpp"

#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <QFileInfo>
#include <QUrl>
namespace Scenario
{

MagneticStates magneticStates(
    MagneticStates cur, Scenario::Point pt, const Scenario::ScenarioPresenter& pres)
{
  constexpr int magnetic = 10;
  const auto& scenario = pres.model();

  // Check if we keep the current magnetism
  // if(cur.horizontal)
  // {
  //   const auto state_date = scenario.events.at(cur.horizontal->eventId()).date();

  //   const double rel_y_distance = std::abs(cur.horizontal->heightPercentage() - pt.y);
  //   const double abs_y_distance = rel_y_distance * pres.view().height();

  //   if(abs_y_distance < magnetic && state_date < pt.date)
  //   {
  //     return {cur.horizontal, cur.vertical, true};
  //   }
  // }
  // else if(cur.vertical)
  // {
  //   auto cur_date = Scenario::parentEvent(*cur.vertical, scenario).date();
  //   const double abs_x_distance
  //       = std::abs((cur_date.impl - pt.date.impl) / pres.zoomRatio());
  //   if(abs_x_distance < magnetic)
  //   {
  //     return {cur.horizontal, cur.vertical, true};
  //   }
  // }

  EventModel& start_ev = scenario.startEvent();
  SCORE_ASSERT(!start_ev.states().empty());

  static ossia::hash_map<Id<EventModel>, ossia::time_value> eventDates;
  eventDates.clear();

  for(auto& ev : scenario.events)
    eventDates[ev.id()] = ev.date();

  Scenario::StateModel* start_st = &scenario.states.at(start_ev.states().front());

  const ossia::time_value pt_msec = pt.date;
  StateModel* min_x_state = start_st;
  double min_x_distance = std::numeric_limits<double>::max();

  StateModel* min_y_state = start_st;
  double min_y_distance = std::numeric_limits<double>::max();
  StateModel* closest_x_y_state = start_st;
  QPointF closest_x_y_distance
      = {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()};

  for(auto& state : scenario.states)
  {
    const auto state_date = eventDates[state.eventId()];

    if(state_date >= pt_msec)
      continue;

    const TimeVal rel_x_distance{pt.date.impl - state_date.impl};
    const double abs_x_distance = rel_x_distance.toPixels(pres.zoomRatio());

    const double rel_y_distance = std::abs(state.heightPercentage() - pt.y);
    const double abs_y_distance = rel_y_distance * pres.view().height();

    if(abs_x_distance < min_x_distance)
    {
      min_x_state = &state;
      min_x_distance = abs_x_distance;
    }

    if(abs_y_distance < min_y_distance)
    {
      min_y_state = &state;
      min_y_distance = abs_y_distance;
    }
  }

  eventDates.clear();
  if(min_x_distance < min_y_distance)
  {
    return {nullptr, min_x_state, true};
  }
  else
  {
    return {min_y_state, nullptr, true};
  }
}

DropHandler::~DropHandler() { }

bool DropHandler::dragEnter(const ScenarioPresenter&, QPointF pos, const QMimeData& mime)
{
  return false;
}

bool DropHandler::dragMove(const ScenarioPresenter&, QPointF pos, const QMimeData& mime)
{
  return false;
}

bool DropHandler::dragLeave(const ScenarioPresenter&, QPointF pos, const QMimeData& mime)
{
  return false;
}

bool DropHandler::canDrop(const QMimeData& mime) const noexcept
{
  return false;
}
GhostIntervalDropHandler::~GhostIntervalDropHandler() { }

bool GhostIntervalDropHandler::dragEnter(
    const Scenario::ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  m_magnetic.vertical = nullptr;
  m_magnetic.horizontal = nullptr;
  return dragMove(pres, pos, mime);
}

bool GhostIntervalDropHandler::dragMove(
    const Scenario::ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  if(!canDrop(mime))
  {
    bool mimeTypes = ossia::any_of(m_acceptableMimeTypes, [&](const auto& mimeType) {
      return mime.formats().contains(mimeType);
    });

    bool suffixes = false;
    for(auto& url : mime.urls())
    {
      if(url.isLocalFile())
      {
        const auto ext = QFileInfo{url.toLocalFile()}.suffix();
        suffixes |= ossia::any_of(m_acceptableSuffixes, [&](const auto& suffix) {
          return ext.compare(suffix, Qt::CaseInsensitive) == 0;
        });
        if(suffixes)
          break;
      }
    }
    if(!(mimeTypes || suffixes))
      return false;
  }

  const auto magnetism = !bool(qApp->keyboardModifiers() & Qt::AltModifier);
  auto pt = pres.toScenarioPoint(pos);
  m_magnetic = magneticStates(m_magnetic, pt, pres);
  auto [x_state, y_state, magnetic] = m_magnetic;

  if(magnetism && y_state)
  {
    if(magnetic)
    {
      // TODO in the drop, handle the case where rel_t < 0 - else, negative
      // date + crash
      pres.drawDragLine(
          *y_state, {Scenario::parentEvent(*y_state, pres.model()).date(), pt.y});
    }
    else
    {
      pres.drawDragLine(*y_state, pt);
    }
  }
  else if(magnetism && x_state)
  {
    if(x_state->nextInterval() || x_state->eventId() == pres.model().startEvent().id())
    {
      pres.drawDragLine(*x_state, pt);
    }
    else
    {
      if(magnetic)
      {
        pres.drawDragLine(*x_state, {pt.date, x_state->heightPercentage()});
      }
      else
      {
        pres.drawDragLine(*x_state, pt);
      }
    }
  }
  else
  {
    m_magnetic = {};
    pres.stopDrawDragLine();
  }
  return true;
}

bool GhostIntervalDropHandler::dragLeave(
    const Scenario::ScenarioPresenter& pres, QPointF pos, const QMimeData& mime)
{
  m_magnetic.vertical = nullptr;
  m_magnetic.horizontal = nullptr;
  pres.stopDrawDragLine();
  return false;
}

DropHandlerList::~DropHandlerList() { }

bool DropHandlerList::dragEnter(
    const ScenarioPresenter& scen, QPointF drop, const QMimeData& mime) const
{
  for(auto& fact : *this)
  {
    if(fact.dragEnter(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::dragMove(
    const ScenarioPresenter& scen, QPointF drop, const QMimeData& mime) const
{
  for(auto& fact : *this)
  {
    if(fact.dragMove(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::dragLeave(
    const ScenarioPresenter& scen, QPointF drop, const QMimeData& mime) const
{
  for(auto& fact : *this)
  {
    if(fact.dragLeave(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::drop(
    const ScenarioPresenter& scen, QPointF drop, const QMimeData& mime) const
{
  for(auto& fact : *this)
  {
    if(fact.drop(scen, drop, mime))
      return true;
  }

  return false;
}

IntervalDropHandler::~IntervalDropHandler() { }

IntervalDropHandlerList::~IntervalDropHandlerList() { }

bool IntervalDropHandlerList::drop(
    const score::DocumentContext& ctx, const Scenario::IntervalModel& cst, QPointF pos,
    const QMimeData& mime) const
{
  for(auto& fact : *this)
  {
    if(fact.drop(ctx, cst, pos, mime))
      return true;
  }

  return false;
}
}
