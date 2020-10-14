// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDisplayedElementsProvider.hpp"

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/Optional.hpp>

#include <QObject>

namespace Scenario
{
bool ScenarioDisplayedElementsProvider::matches(const IntervalModel& cst) const
{
  return dynamic_cast<Scenario::ProcessModel*>(cst.parent());
}

DisplayedElementsContainer ScenarioDisplayedElementsProvider::make(IntervalModel& cst) const
{
  if (auto parent_scenario = dynamic_cast<Scenario::ProcessModel*>(cst.parent()))
  {
    auto& sst = parent_scenario->states.at(cst.startState());
    auto& est = parent_scenario->states.at(cst.endState());
    auto& sev = parent_scenario->events.at(sst.eventId());
    auto& eev = parent_scenario->events.at(est.eventId());
    return DisplayedElementsContainer{
        cst,
        sst,
        est,
        sev,
        eev,
        parent_scenario->timeSyncs.at(sev.timeSync()),
        parent_scenario->timeSyncs.at(eev.timeSync())};
  }

  return {};
}

DisplayedElementsPresenterContainer ScenarioDisplayedElementsProvider::make_presenters(
    const IntervalModel& m,
    const Process::Context& ctx,
    QGraphicsItem* view_parent,
    QObject* parent) const
{
  if (auto sm = dynamic_cast<Scenario::ProcessModel*>(m.parent()))
  {
    const auto& startState = sm->states.at(m.startState());
    const auto& endState = sm->states.at(m.endState());
    const auto& startEvent = sm->events.at(startState.eventId());
    const auto& endEvent = sm->events.at(endState.eventId());
    const auto& startNode = sm->timeSyncs.at(startEvent.timeSync());
    const auto& endNode = sm->timeSyncs.at(endEvent.timeSync());
    return DisplayedElementsPresenterContainer{
        new FullViewIntervalPresenter{m, ctx, view_parent, parent},
        new StatePresenter{startState, ctx, view_parent, parent},
        new StatePresenter{endState, ctx, view_parent, parent},
        new EventPresenter{startEvent, view_parent, parent},
        new EventPresenter{endEvent, view_parent, parent},
        new TimeSyncPresenter{startNode, view_parent, parent},
        new TimeSyncPresenter{endNode, view_parent, parent}};
  }
  return {};
}

bool DefaultDisplayedElementsProvider::matches(const IntervalModel& cst) const
{
  return dynamic_cast<Scenario::ScenarioInterface*>(cst.parent());
}

DisplayedElementsContainer DefaultDisplayedElementsProvider::make(IntervalModel& cst) const
{
  if (auto parent_scenario = dynamic_cast<Scenario::ScenarioInterface*>(cst.parent()))
  {
    auto& sst = parent_scenario->state(cst.startState());
    auto& est = parent_scenario->state(cst.endState());
    auto& sev = parent_scenario->event(sst.eventId());
    auto& eev = parent_scenario->event(est.eventId());
    return DisplayedElementsContainer{
        cst,
        sst,
        est,
        sev,
        eev,
        parent_scenario->timeSync(sev.timeSync()),
        parent_scenario->timeSync(eev.timeSync())};
  }

  return {};
}

DisplayedElementsPresenterContainer DefaultDisplayedElementsProvider::make_presenters(
    const IntervalModel& m,
    const Process::Context& ctx,
    QGraphicsItem* view_parent,
    QObject* parent) const
{
  if (auto sm = dynamic_cast<Scenario::ScenarioInterface*>(m.parent()))
  {
    const auto& startState = sm->state(m.startState());
    const auto& endState = sm->state(m.endState());
    const auto& startEvent = sm->event(startState.eventId());
    const auto& endEvent = sm->event(endState.eventId());
    const auto& startNode = sm->timeSync(startEvent.timeSync());
    const auto& endNode = sm->timeSync(endEvent.timeSync());
    return DisplayedElementsPresenterContainer{
        new FullViewIntervalPresenter{m, ctx, view_parent, parent},
        new StatePresenter{startState, ctx, view_parent, parent},
        new StatePresenter{endState, ctx, view_parent, parent},
        new EventPresenter{startEvent, view_parent, parent},
        new EventPresenter{endEvent, view_parent, parent},
        new TimeSyncPresenter{startNode, view_parent, parent},
        new TimeSyncPresenter{endNode, view_parent, parent}};
  }
  return {};
}
}
