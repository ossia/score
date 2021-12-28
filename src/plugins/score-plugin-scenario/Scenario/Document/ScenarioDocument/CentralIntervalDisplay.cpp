#include <Scenario/Document/ScenarioDocument/CentralIntervalDisplay.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactoryList.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>

#include <Process/Commands/EditPort.hpp>
#include <Library/ProcessesItemModel.hpp>

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
      &DisplayedElementsToolPaletteFactory::make,
      parent,
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
      &parent.focusManager(),
      static_cast<void (Process::ProcessFocusManager::*)(
          QPointer<Process::LayerPresenter>)>(
          &Process::ProcessFocusManager::focus));
}

Process::ProcessModel* closestParentProcessBeforeInterval(const QObject* obj)
{
  if(auto p = qobject_cast<const Process::ProcessModel*>(obj))
    return const_cast<Process::ProcessModel*>(p);
  else if(auto p = qobject_cast<const Scenario::IntervalModel*>(obj))
    return nullptr;
  else if(auto parent = obj->parent())
    return closestParentProcessBeforeInterval(parent);
  else
    return nullptr;
}

void createProcessAfterPort(
    Scenario::ScenarioDocumentPresenter& parent,
    const Library::ProcessData& dat,
    const Process::ProcessModel& parentProcess,
    const Process::Port& p)
{
  if(auto parent_itv = Scenario::closestParentInterval(const_cast<Process::Port*>(&p)))
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};

    auto pos = parentProcess.position();
    pos.rx() += parentProcess.size().width() + 40;
    auto proc = m.createProcessInNewSlot(*parent_itv, dat, pos);
    if(proc)
    {
      // TODO all of this should be made atomic...
      if(!proc->inlets().empty())
      {
        auto new_inlet = proc->inlets()[0];
        // Create a cable from the output to the input
        if(new_inlet->type() == p.type())
        {
          m.createCable(parent.model(), p, *proc->inlets()[0]);
        }

        if(!proc->outlets().empty())
        {
          auto new_outlet = proc->outlets()[0];
          // Move the address in the selected output to the first outlet of the new process
          if(new_outlet->type() == p.type())
          {
            if(auto addr = p.address(); addr != State::AddressAccessor{})
            {
              m.setProperty<Process::Port::p_address>(*new_outlet, addr);
              m.setProperty<Process::Port::p_address>(p, State::AddressAccessor{});
            }
          }
        }
        parent.context().selectionStack.pushNewSelection({proc});
      }
    }
    m.commit();
  }
}
void CentralIntervalDisplay::on_addProcessFromLibrary(const Library::ProcessData& dat)
{
  // First try to see if an interval is selected.
  auto createInParentInterval = [&]
  {
    auto sel = filterSelectionByType<IntervalModel>(
        parent.context().selectionStack.currentSelection());
    if (sel.size() == 1)
    {
      const Scenario::IntervalModel& itv = *sel.front();

      Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};
      m.createProcessInNewSlot(itv, dat);
      m.commit();
      return;
    }
  };

  createInParentInterval();

  // Else try to see if a process is selected.
  {
    auto sel = filterSelectionByType<Process::ProcessModel>(
          parent.context().selectionStack.currentSelection());
    if (sel.size() == 1)
    {
      const Process::ProcessModel& parentProcess = *sel.front();
      if(!parentProcess.outlets().empty())
      {
        createProcessAfterPort(parent, dat, parentProcess, *parentProcess.outlets().front());
      }
      else
      {
        createInParentInterval();
      }

      return;
    }
  }
  // Else try to see if a port is selected.
  {
    auto sel = filterSelectionByType<Process::Port>(
        parent.context().selectionStack.currentSelection());
    if (sel.size() == 1)
    {
      const Process::Port& p = *sel.front();
      auto parentProcess = closestParentProcessBeforeInterval(&p);
      if(parentProcess)
        createProcessAfterPort(parent, dat, *parentProcess, p);
      else
        createInParentInterval();
      return;
    }
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
