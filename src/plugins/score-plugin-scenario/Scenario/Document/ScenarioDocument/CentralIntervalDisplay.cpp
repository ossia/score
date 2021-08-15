#include <Scenario/Document/ScenarioDocument/CentralIntervalDisplay.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>

#include <Library/ProcessesItemModel.hpp>

#include <score/selection/SelectionStack.hpp>

#include <QScrollBar>

namespace Scenario
{

CentralIntervalDisplay::CentralIntervalDisplay(ScenarioDocumentPresenter& p)
    : parent{p}
    , presenter{p}
{
  auto& view = parent.view();
  auto& gv = view.view();
  auto& interval = p.displayedElements.interval();
  // Setup of the state machine.
  const auto& fact
      = parent.context().app.interfaces<DisplayedElementsToolPaletteFactoryList>();
  m_stateMachine = fact.make(
      &DisplayedElementsToolPaletteFactory::make,
      p,
      presenter,
      interval,
      &view.baseItem());

  // Creation of the presenters
  parent.m_updatingView = true;
  presenter.on_displayedIntervalChanged(interval);
  parent.m_updatingView = false;
  auto itv_p = presenter.intervalPresenter();

  SCORE_ASSERT(itv_p);
  QObject::connect(itv_p, &FullViewIntervalPresenter::intervalSelected, &parent, &ScenarioDocumentPresenter::setDisplayedInterval);

  parent.on_viewReady();
  parent.updateMinimap();
  gv.verticalScrollBar()->setValue(0);

  view.timeRuler().setGrid(itv_p->grid());
  QObject::connect(
      &presenter,
      &DisplayedElementsPresenter::requestFocusedPresenterChange,
      &p.focusManager(),
      static_cast<void (Process::ProcessFocusManager::*)(
          QPointer<Process::LayerPresenter>)>(
          &Process::ProcessFocusManager::focus));
}

CentralIntervalDisplay::~CentralIntervalDisplay()
{
  presenter.remove();
}

void CentralIntervalDisplay::on_addProcessFromLibrary(const Library::ProcessData& dat)
{
  auto sel = filterSelectionByType<IntervalModel>(
      parent.context().selectionStack.currentSelection());
  if (sel.size() == 1)
  {
    const Scenario::IntervalModel& itv = *sel.front();

    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};
    m.createProcessInNewSlot(itv, dat.key, dat.customData);
    m.commit();
  }
}

void CentralIntervalDisplay::on_visibleRectChanged(const QRectF&)
{
  auto& gv = parent.view().view();
  if (auto p = presenter.intervalPresenter())
    p->on_visibleRectChanged(gv.visibleRect());
}

void CentralIntervalDisplay::on_executionTimer()
{

}

}
