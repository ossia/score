// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LoopDisplayedElements.hpp"

#include "LoopProcessModel.hpp"

#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
namespace Loop
{
bool DisplayedElementsProvider::matches(const Scenario::IntervalModel& cst) const
{
  return dynamic_cast<Loop::ProcessModel*>(cst.parent());
}

Scenario::DisplayedElementsContainer
DisplayedElementsProvider::make(Scenario::IntervalModel& cst) const
{
  if (auto parent_base = dynamic_cast<Loop::ProcessModel*>(cst.parent()))
  {
    return Scenario::DisplayedElementsContainer{
        cst,
        parent_base->startState(),
        parent_base->endState(),

        parent_base->startEvent(),
        parent_base->endEvent(),

        parent_base->startTimeSync(),
        parent_base->endTimeSync()};
  }

  return {};
}
Scenario::DisplayedElementsPresenterContainer DisplayedElementsProvider::make_presenters(
    const Scenario::IntervalModel& m,
    const Process::Context& ctx,
    QGraphicsItem* view_parent,
    QObject* parent) const
{
  if (auto bs = dynamic_cast<Loop::ProcessModel*>(m.parent()))
  {
    return Scenario::DisplayedElementsPresenterContainer{
        new Scenario::FullViewIntervalPresenter{m, ctx, view_parent, parent},
        new Scenario::StatePresenter{bs->startState(), ctx, view_parent, parent},
        new Scenario::StatePresenter{bs->endState(), ctx, view_parent, parent},
        new Scenario::EventPresenter{bs->startEvent(), view_parent, parent},
        new Scenario::EventPresenter{bs->endEvent(), view_parent, parent},
        new Scenario::TimeSyncPresenter{bs->startTimeSync(), view_parent, parent},
        new Scenario::TimeSyncPresenter{bs->endTimeSync(), view_parent, parent}};
  }
  return {};
}
}
