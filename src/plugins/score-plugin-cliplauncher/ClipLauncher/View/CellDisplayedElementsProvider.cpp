#include "CellDisplayedElementsProvider.hpp"

#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>

#include <ClipLauncher/CellModel.hpp>

namespace ClipLauncher
{

bool CellDisplayedElementsProvider::matches(
    const Scenario::IntervalModel& cst) const
{
  return dynamic_cast<CellModel*>(cst.parent()) != nullptr;
}

Scenario::DisplayedElementsContainer
CellDisplayedElementsProvider::make(Scenario::IntervalModel& cst) const
{
  if(auto* cell = dynamic_cast<CellModel*>(cst.parent()))
  {
    auto& sc = cell->scenarioContainer();
    return Scenario::DisplayedElementsContainer{
        cst,
        sc.startState(),
        sc.endState(),
        sc.startEvent(),
        sc.endEvent(),
        sc.startTimeSync(),
        sc.endTimeSync()};
  }
  return {};
}

Scenario::DisplayedElementsPresenterContainer
CellDisplayedElementsProvider::make_presenters(
    ZoomRatio zoom, const Scenario::IntervalModel& m,
    const Process::Context& ctx, QGraphicsItem* view_parent,
    QObject* parent) const
{
  if(auto* cell = dynamic_cast<CellModel*>(m.parent()))
  {
    auto& sc = cell->scenarioContainer();
    return Scenario::DisplayedElementsPresenterContainer{
        new Scenario::FullViewIntervalPresenter{zoom, m, ctx, view_parent, parent},
        new Scenario::StatePresenter{sc.startState(), ctx, view_parent, parent},
        new Scenario::StatePresenter{sc.endState(), ctx, view_parent, parent},
        new Scenario::EventPresenter{sc.startEvent(), view_parent, parent},
        new Scenario::EventPresenter{sc.endEvent(), view_parent, parent},
        new Scenario::TimeSyncPresenter{sc.startTimeSync(), view_parent, parent},
        new Scenario::TimeSyncPresenter{sc.endTimeSync(), view_parent, parent}};
  }
  return {};
}

} // namespace ClipLauncher
