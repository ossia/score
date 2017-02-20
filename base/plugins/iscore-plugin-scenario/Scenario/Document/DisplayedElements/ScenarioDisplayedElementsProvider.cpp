#include <QObject>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "ScenarioDisplayedElementsProvider.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
bool ScenarioDisplayedElementsProvider::matches(
    const ConstraintModel& cst) const
{
  return dynamic_cast<Scenario::ProcessModel*>(cst.parent());
}

DisplayedElementsContainer
ScenarioDisplayedElementsProvider::make(ConstraintModel& cst) const
{
  if (auto parent_scenario
      = dynamic_cast<Scenario::ProcessModel*>(cst.parent()))
  {
    const auto& sst = parent_scenario->states.at(cst.startState());
    const auto& est = parent_scenario->states.at(cst.endState());
    const auto& sev = parent_scenario->events.at(sst.eventId());
    const auto& eev = parent_scenario->events.at(est.eventId());
    return DisplayedElementsContainer{
        cst,
        sst,
        est,
        sev,
        eev,
        parent_scenario->timeNodes.at(sev.timeNode()),
        parent_scenario->timeNodes.at(eev.timeNode())};
  }

  return {};
}

DisplayedElementsPresenterContainer
ScenarioDisplayedElementsProvider::make_presenters(
    const ConstraintModel& m,
    const Process::ProcessPresenterContext& ctx,
    QQuickPaintedItem* view_parent,
    QObject* parent) const
{
  if (auto sm = dynamic_cast<Scenario::ProcessModel*>(m.parent()))
  {
    const auto& startState = sm->states.at(m.startState());
    const auto& endState = sm->states.at(m.endState());
    const auto& startEvent = sm->events.at(startState.eventId());
    const auto& endEvent = sm->events.at(endState.eventId());
    const auto& startNode = sm->timeNodes.at(startEvent.timeNode());
    const auto& endNode = sm->timeNodes.at(endEvent.timeNode());
    return DisplayedElementsPresenterContainer{
        new FullViewConstraintPresenter{*m.fullView(), ctx, view_parent,
                                        parent},
        new StatePresenter{startState, view_parent, parent},
        new StatePresenter{endState, view_parent, parent},
        new EventPresenter{startEvent, view_parent, parent},
        new EventPresenter{endEvent, view_parent, parent},
        new TimeNodePresenter{startNode, view_parent, parent},
        new TimeNodePresenter{endNode, view_parent, parent}};
  }
  return {};
}
}
