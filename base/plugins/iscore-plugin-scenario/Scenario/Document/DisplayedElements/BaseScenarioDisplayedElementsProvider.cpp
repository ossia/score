#include <QObject>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>

#include "BaseScenarioDisplayedElementsProvider.hpp"
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>

namespace Scenario
{
bool BaseScenarioDisplayedElementsProvider::matches(
    const ConstraintModel& cst) const
{
  return dynamic_cast<BaseScenario*>(cst.parent());
}

DisplayedElementsContainer
BaseScenarioDisplayedElementsProvider::make(ConstraintModel& cst) const
{
  if (auto parent_base = dynamic_cast<BaseScenario*>(cst.parent()))
  {
    return DisplayedElementsContainer{cst,
                                      parent_base->startState(),
                                      parent_base->endState(),

                                      parent_base->startEvent(),
                                      parent_base->endEvent(),

                                      parent_base->startTimeNode(),
                                      parent_base->endTimeNode()};
  }

  return {};
}

DisplayedElementsPresenterContainer
BaseScenarioDisplayedElementsProvider::make_presenters(
    const ConstraintModel& m,
    const Process::ProcessPresenterContext& ctx,
    QQuickPaintedItem* view_parent,
    QObject* parent) const
{
  if (auto bs = dynamic_cast<BaseScenario*>(m.parent()))
  {
    return DisplayedElementsPresenterContainer{
        new FullViewConstraintPresenter{*m.fullView(), ctx, view_parent,
                                        parent},
        new StatePresenter{bs->startState(), view_parent, parent},
        new StatePresenter{bs->endState(), view_parent, parent},
        new EventPresenter{bs->startEvent(), view_parent, parent},
        new EventPresenter{bs->endEvent(), view_parent, parent},
        new TimeNodePresenter{bs->startTimeNode(), view_parent, parent},
        new TimeNodePresenter{bs->endTimeNode(), view_parent, parent}};
  }
  return {};
}
}
