#include <Scenario/Document/ScenarioDocument/CentralNodalDisplay.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/Interval/FullView/NodalIntervalView.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/CommandAPI.hpp>

#include <Library/ProcessesItemModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionStack.hpp>

#include <QMenu>
#include <QTimer>

namespace Scenario
{

CentralNodalDisplay::CentralNodalDisplay(ScenarioDocumentPresenter& p)
    : parent{p}
{
}

void CentralNodalDisplay::init()
{
  auto& itv = parent.displayedInterval();
  auto& view = parent.view();
  ProcessGraphicsView& gv = view.view();
  presenter = new NodalIntervalView{
      NodalIntervalView::AllItems,
      itv,
      parent.context(),
      &view.baseItem()};

  view.view().setSceneRect(QRectF{0, 0, 10, 10});
  con(gv, &ProcessGraphicsView::dropRequested, presenter, [this, &gv](QPoint viewPos, const QMimeData* data) {
        auto sp = gv.mapToScene(viewPos);
        auto ip = presenter->mapFromScene(sp);
        presenter->on_drop(ip, data);
      });

  con(gv, &ProcessGraphicsView::emptyContextMenuRequested, presenter, [this, &gv](const QPoint& pos) {
        QMenu contextMenu(&gv);
        auto recenter = contextMenu.addAction(QObject::tr("Recenter"));

        auto act = contextMenu.exec(gv.mapToGlobal(pos));
        if (act == recenter)
          this->recenter();
      });

  QTimer::singleShot(0, presenter, &NodalIntervalView::recenter);
}

void CentralNodalDisplay::recenter()
{
  const auto& nodes = presenter->enclosingRect();
  auto& gv = parent.view().view();
  gv.centerOn(presenter->mapToScene(nodes.center()));
}

void CentralNodalDisplay::on_addProcessFromLibrary(const Library::ProcessData& dat)
{
  auto createInParentInterval = [&]
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};
    m.createProcessInNewSlot(parent.displayedInterval(), dat);
    m.commit();
  };

  // Try to see if a process is selected.
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

  // Else just create there
  createInParentInterval();
}

void CentralNodalDisplay::on_addPresetFromLibrary(const Process::Preset& dat)
{
  auto createInParentInterval = [&]
  {
    Command::Macro m{new Command::DropProcessInIntervalMacro, parent.context()};
    m.loadProcessFromPreset(parent.displayedInterval(), dat);
    m.commit();
  };

  // Try to see if a process is selected.
  {
    auto sel = filterSelectionByType<Process::ProcessModel>(
          parent.context().selectionStack.currentSelection());
    if (sel.size() == 1)
    {
      const Process::ProcessModel& parentProcess = *sel.front();
      if(!parentProcess.outlets().empty())
      {
        loadPresetAfterPort(parent, dat, parentProcess, *parentProcess.outlets().front());
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
        loadPresetAfterPort(parent, dat, *parentProcess, p);
      else
        createInParentInterval();
      return;
    }
  }

  // Else just create there
  createInParentInterval();
}

void CentralNodalDisplay::on_visibleRectChanged(const QRectF&)
{

}

void CentralNodalDisplay::on_executionTimer()
{
  auto& itv = parent.displayedInterval();
  auto pctg = itv.duration.playPercentage();
  presenter->on_playPercentageChanged(
      pctg, itv.duration.defaultDuration());
}

CentralNodalDisplay::~CentralNodalDisplay()
{
  delete presenter;
}
}
