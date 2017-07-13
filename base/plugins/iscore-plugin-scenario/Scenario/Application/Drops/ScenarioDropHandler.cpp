// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
    const TemporalScenarioPresenter& scen,
    QPointF drop,
    const QMimeData* mime) const
{
  for (auto& fact : *this)
  {
    if (fact.dragEnter(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::dragMove(
    const TemporalScenarioPresenter& scen,
    QPointF drop,
    const QMimeData* mime) const
{
  for (auto& fact : *this)
  {
    if (fact.dragMove(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::dragLeave(
    const TemporalScenarioPresenter& scen,
    QPointF drop,
    const QMimeData* mime) const
{
  for (auto& fact : *this)
  {
    if (fact.dragLeave(scen, drop, mime))
      return true;
  }

  return false;
}

bool DropHandlerList::drop(
    const TemporalScenarioPresenter& scen,
    QPointF drop,
    const QMimeData* mime) const
{
  for (auto& fact : *this)
  {
    if (fact.drop(scen, drop, mime))
      return true;
  }

  return false;
}

ConstraintDropHandler::~ConstraintDropHandler()
{
}

ConstraintDropHandlerList::~ConstraintDropHandlerList()
{
}

bool ConstraintDropHandlerList::drop(
    const Scenario::ConstraintModel& cst, const QMimeData* mime) const
{
  for (auto& fact : *this)
  {
    if (fact.drop(cst, mime))
      return true;
  }

  return false;
}
}
