#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Document/ScenarioDocument/CentralIntervalDisplay.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessCreation.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>

#include <Library/ProcessesItemModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/selection/SelectionStack.hpp>

#include <QScrollBar>

namespace Scenario
{

CentralIntervalDisplay::CentralIntervalDisplay(ScenarioDocumentPresenter& p)
    : parent{p}
    , presenter{p}
{
}

CentralIntervalDisplay::~CentralIntervalDisplay()
{
  auto& view = parent.view();
  auto& gv = view.view();
  gv.autoScrollHandler = {};
  if(auto itv_p = presenter.intervalPresenter())
  {
    QObject::disconnect(
        itv_p, &FullViewIntervalPresenter::intervalSelected, &parent,
        &ScenarioDocumentPresenter::setDisplayedInterval);
  }
  QObject::disconnect(
      &gv, &ProcessGraphicsView::dropRequested, &parent,
      &ScenarioDocumentPresenter::on_dropInEmptyFullView);

  QObject::disconnect(
      &presenter, &DisplayedElementsPresenter::requestFocusedPresenterChange,
      &parent.focusManager(),
      static_cast<void (Process::ProcessFocusManager::*)(
          QPointer<Process::LayerPresenter>)>(&Process::ProcessFocusManager::focus));

  presenter.remove();
}

void CentralIntervalDisplay::init()
{
  auto& view = parent.view();
  auto& gv = view.view();
  auto& interval = parent.displayedElements.interval();
  // Setup of the state machine.
  const auto& fact
      = parent.context().app.interfaces<DisplayedElementsToolPaletteFactoryList>();
  m_stateMachine = fact.make(
      &DisplayedElementsToolPaletteFactory::make, parent, presenter, interval,
      &view.baseItem());

  // Creation of the presenters
  parent.m_updatingView = true;
  presenter.on_displayedIntervalChanged(interval);
  parent.m_updatingView = false;
  auto itv_p = presenter.intervalPresenter();

  SCORE_ASSERT(itv_p);
  QObject::connect(
      itv_p, &FullViewIntervalPresenter::intervalSelected, &parent,
      &ScenarioDocumentPresenter::setDisplayedInterval, Qt::UniqueConnection);

  parent.on_viewReady();
  parent.updateMinimap();
  gv.verticalScrollBar()->setValue(0);

  // Revealing what lies further: the timeline grows past its end, like it
  // does when the scroll bar is dragged against its end (the presenter then
  // extends the interval's gui duration on scrolled()); vertically, the
  // scroll bar is all there is.
  gv.autoScrollHandler = [&gv](QPoint delta) {
    bool moved = false;
    if(delta.x() != 0)
    {
      auto hsb = gv.horizontalScrollBar();
      if(delta.x() > 0 && hsb->value() + delta.x() > hsb->maximum())
      {
        auto r = gv.sceneRect();
        r.adjust(0, 0, delta.x(), 0);
        gv.setSceneRect(r);
      }
      const int old = hsb->value();
      hsb->setValue(old + delta.x());
      moved |= hsb->value() != old;
    }
    if(delta.y() != 0)
    {
      auto vsb = gv.verticalScrollBar();
      const int old = vsb->value();
      vsb->setValue(old + delta.y());
      moved |= vsb->value() != old;
    }
    return moved;
  };

  view.timeRuler().setGrid(itv_p->grid());
  QObject::connect(
      &presenter, &DisplayedElementsPresenter::requestFocusedPresenterChange,
      &parent.focusManager(),
      static_cast<void (Process::ProcessFocusManager::*)(
          QPointer<Process::LayerPresenter>)>(&Process::ProcessFocusManager::focus),
      Qt::UniqueConnection);

  // In case we drop in the emptiness in the interval view, we want to
  // create a node there and an appropriately-sized slot
  QObject::connect(
      &gv, &ProcessGraphicsView::dropRequested, &parent,
      &ScenarioDocumentPresenter::on_dropInEmptyFullView, Qt::UniqueConnection);
}

Process::ProcessModel* closestParentProcessBeforeInterval(const QObject* obj)
{
  if(auto p = qobject_cast<const Process::ProcessModel*>(obj))
    return const_cast<Process::ProcessModel*>(p);
  else if(qobject_cast<const Scenario::IntervalModel*>(obj))
    return nullptr;
  else if(auto parent = obj->parent())
    return closestParentProcessBeforeInterval(parent);
  else
    return nullptr;
}

void CentralIntervalDisplay::on_addProcessFromLibrary(const Library::ProcessData& dat)
{
  // First try to see if an interval is selected.
  auto createInParentInterval = [&] {
    auto sel = filterSelectionByType<IntervalModel>(
        parent.context().selectionStack.currentSelection());
    if(sel.size() == 1)
    {
      const Scenario::IntervalModel& itv = *sel.front();

      Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};
      m.createProcessInNewSlot(itv, dat);
      m.commit();
      return true;
    }
    return false;
  };

  if(createInParentInterval())
    return;

  // Else try to see if a cable is selected.
  {
    auto sel = filterSelectionByType<Process::Cable>(
        parent.context().selectionStack.currentSelection());
    if(sel.size() == 1)
    {
      const Process::Cable& cbl = *sel.front();
      createProcessInCable(parent.context(), parent.model(), dat, {}, {}, cbl);
      return;
    }
  }

  // Else try to see if a process is selected.
  {
    auto sel = filterSelectionByType<Process::ProcessModel>(
        parent.context().selectionStack.currentSelection());
    if(sel.size() == 1)
    {
      const Process::ProcessModel& parentProcess = *sel.front();
      if(!parentProcess.outlets().empty())
      {
        createProcessAfterPort(
            parent, dat, {}, {}, parentProcess, *parentProcess.outlets().front());
      }
      else
      {
        if(createInParentInterval())
          return;
      }

      return;
    }
  }
  // Else try to see if a port is selected.
  {
    auto sel = filterSelectionByType<Process::Port>(
        parent.context().selectionStack.currentSelection());
    if(sel.size() == 1)
    {
      const Process::Port& p = *sel.front();
      auto parentProcess = closestParentProcessBeforeInterval(&p);
      if(parentProcess)
      {
        if(auto inl = qobject_cast<const Process::Inlet*>(&p))
          createProcessBeforePort(parent, dat, {}, {}, *parentProcess, *inl);
        else if(auto inl = qobject_cast<const Process::Outlet*>(&p))
          createProcessAfterPort(parent, dat, {}, {}, *parentProcess, *inl);
      }
      else if(createInParentInterval())
        return;
      return;
    }
  }
}

void CentralIntervalDisplay::on_addPresetFromLibrary(const Process::Preset& dat)
{
  // First try to see if an interval is selected.
  auto createInParentInterval = [&] {
    auto sel = filterSelectionByType<IntervalModel>(
        parent.context().selectionStack.currentSelection());
    if(sel.size() == 1)
    {
      const Scenario::IntervalModel& itv = *sel.front();

      Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};
      m.loadProcessFromPreset(itv, dat);
      m.commit();
      return true;
    }
    return false;
  };

  if(createInParentInterval())
    return;

  // Else try to see if a cable is selected.
  {
    auto sel = filterSelectionByType<Process::Cable>(
        parent.context().selectionStack.currentSelection());
    if(sel.size() == 1)
    {
      const Process::Cable& cbl = *sel.front();
      loadPresetInCable(parent.context(), parent.model(), dat, cbl);
      return;
    }
  }

  // Else try to see if a process is selected.
  {
    auto sel = filterSelectionByType<Process::ProcessModel>(
        parent.context().selectionStack.currentSelection());
    if(sel.size() == 1)
    {
      const Process::ProcessModel& parentProcess = *sel.front();
      if(!parentProcess.outlets().empty())
      {
        loadPresetAfterPort(
            parent, dat, parentProcess, *parentProcess.outlets().front());
      }
      else
      {
        if(createInParentInterval())
          return;
      }

      return;
    }
  }
  // Else try to see if a port is selected.
  {
    auto sel = filterSelectionByType<Process::Port>(
        parent.context().selectionStack.currentSelection());
    if(sel.size() == 1)
    {
      const Process::Port& p = *sel.front();
      auto parentProcess = closestParentProcessBeforeInterval(&p);

      if(parentProcess)
      {
        if(auto inl = qobject_cast<const Process::Inlet*>(&p))
          loadPresetBeforePort(parent, dat, *parentProcess, *inl);
        else if(auto inl = qobject_cast<const Process::Outlet*>(&p))
          loadPresetAfterPort(parent, dat, *parentProcess, *inl);
      }
      else if(createInParentInterval())
        return;
      return;
    }
  }
}

void CentralIntervalDisplay::on_visibleRectChanged(const QRectF&)
{
  auto& gv = parent.view().view();
  if(auto p = presenter.intervalPresenter())
    p->on_visibleRectChanged(gv.visibleRect());
}

void CentralIntervalDisplay::on_executionTimer() { }

}
