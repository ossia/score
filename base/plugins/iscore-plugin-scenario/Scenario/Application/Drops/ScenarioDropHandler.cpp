#include "ScenarioDropHandler.hpp"

namespace Scenario
{

DropHandler::~DropHandler()
{
}

DropHandlerList::~DropHandlerList()
{
}

bool DropHandlerList::handle(
    const TemporalScenarioPresenter& scen,
    QPointF drop,
    const QMimeData* mime) const
{
  for (auto& fact : *this)
  {
    if (fact.handle(scen, drop, mime))
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

bool ConstraintDropHandlerList::handle(
    const Scenario::ConstraintModel& cst, const QMimeData* mime) const
{
  for (auto& fact : *this)
  {
    if (fact.handle(cst, mime))
      return true;
  }

  return false;
}
}
