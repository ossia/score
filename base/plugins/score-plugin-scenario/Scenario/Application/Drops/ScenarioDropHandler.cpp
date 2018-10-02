// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDropHandler.hpp"

namespace Scenario
{

DropHandler::~DropHandler()
{
}

DropHandlerList::~DropHandlerList()
{
}

bool DropHandlerList::dragEnter(
    const ScenarioPresenter& scen, QPointF drop,
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
    const ScenarioPresenter& scen, QPointF drop,
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
    const ScenarioPresenter& scen, QPointF drop,
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
    const ScenarioPresenter& scen, QPointF drop,
    const QMimeData& mime) const
{
  for (auto& fact : *this)
  {
    if (fact.drop(scen, drop, mime))
      return true;
  }

  return false;
}

IntervalDropHandler::~IntervalDropHandler()
{
}

IntervalDropHandlerList::~IntervalDropHandlerList()
{
}

bool IntervalDropHandlerList::drop(
    const Scenario::IntervalModel& cst, const QMimeData& mime) const
{
  for (auto& fact : *this)
  {
    if (fact.drop(cst, mime))
      return true;
  }

  return false;
}
}
