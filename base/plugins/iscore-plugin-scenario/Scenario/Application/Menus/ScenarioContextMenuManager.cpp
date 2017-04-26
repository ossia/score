
#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Constraint/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/CreateProcessInExistingSlot.hpp>
#include <Scenario/Commands/Constraint/CreateProcessInNewSlot.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Constraint/Rack/RemoveSlotFromRack.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateCommentBlock.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/ViewCommands/PutLayerModelToFront.hpp>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <QPoint>
#include <QString>
#include <algorithm>

#include "ScenarioContextMenuManager.hpp"
#include <Process/ProcessList.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>
#include <Scenario/Document/Constraint/FullView/FullViewConstraintPresenter.hpp>

namespace Scenario
{
void ScenarioContextMenuManager::createSlotContextMenu(
    const iscore::DocumentContext& ctx,
    QMenu& menu,
    const TemporalConstraintPresenter& pres,
    int slot_index)
{
  using namespace Scenario::Command;
  // TODO see
  // http://stackoverflow.com/questions/21443023/capturing-a-reference-by-reference-in-a-c11-lambda
  auto& constraint = pres.model();
  const Slot& slot = constraint.smallView().at(slot_index);
  SlotPath slot_path{constraint, slot_index, Slot::SmallView};

  // First changing the process in the current slot
  auto processes_submenu = menu.addMenu(tr("Focus process in this slot"));
  for (const Id<Process::ProcessModel>& proc : slot.processes)
  {
    auto& p = constraint.processes.at(proc);
    QAction* procAct
        = new QAction{p.prettyName(), processes_submenu};
    QObject::connect(procAct, &QAction::triggered, [&,slot_path] () mutable {
      PutLayerModelToFront cmd{std::move(slot_path), p.id()};
      cmd.redo();
    });
    processes_submenu->addAction(procAct);
  }

  // Then creation of a new slot with existing processes
  auto new_processes_submenu = menu.addMenu(tr("Show process in new slot"));
  for (const Id<Process::ProcessModel>& proc : slot.processes)
  {
    auto& p = constraint.processes.at(proc);
    QAction* procAct
        = new QAction{p.prettyName(), new_processes_submenu};
    QObject::connect(procAct, &QAction::triggered, [&]() {
      auto cmd = new Scenario::Command::AddLayerInNewSlot{
          constraint, p.id()};
      CommandDispatcher<>{ctx.commandStack}.submitCommand(cmd);
    });
    new_processes_submenu->addAction(procAct);
  }

  // Then removal of slot
  auto removeSlotAct = new QAction{tr("Remove this slot"), nullptr};
  QObject::connect(removeSlotAct, &QAction::triggered, [&,slot_path]() {
    auto cmd = new Scenario::Command::RemoveSlotFromRack{slot_path};
    CommandDispatcher<>{ctx.commandStack}.submitCommand(cmd);
  });
  menu.addAction(removeSlotAct);

  menu.addSeparator();

  // Then Add process in this slot
  auto existing_processes_submenu
      = menu.addMenu(tr("Add existing process in this slot"));
  for (const Process::ProcessModel& proc : constraint.processes)
  {
    // OPTIMIZEME by filtering before.
    if (ossia::none_of(slot.processes,
            [&] (const Id<Process::ProcessModel>& layer) {
              return layer == proc.id();
            }))
    {
      QAction* procAct
          = new QAction{proc.prettyName(), existing_processes_submenu};
      QObject::connect(procAct, &QAction::triggered, [&, slot_path] () mutable {
        auto cmd2 = new Scenario::Command::AddLayerModelToSlot{std::move(slot_path), proc};
        CommandDispatcher<>{ctx.commandStack}.submitCommand(cmd2);
      });
      existing_processes_submenu->addAction(procAct);
    }
  }

  auto addNewProcessInExistingSlot
      = new QAction{tr("Add new process in this slot"), &menu};
  QObject::connect(addNewProcessInExistingSlot, &QAction::triggered, [&]() {
    auto& fact = ctx.app.interfaces<Process::ProcessFactoryList>();
    AddProcessDialog dialog{fact, qApp->activeWindow()};

    QObject::connect(
        &dialog, &AddProcessDialog::okPressed, [&, slot_path] (const auto& proc) mutable {
          QuietMacroCommandDispatcher<Scenario::Command::
                                          CreateProcessInExistingSlot>
              disp{ctx.commandStack};

          auto cmd1 = new Scenario::Command::AddOnlyProcessToConstraint(
              constraint, proc);
          cmd1->redo();
          disp.submitCommand(cmd1);

          auto cmd2 = new Scenario::Command::AddLayerModelToSlot(
              std::move(slot_path), constraint.processes.at(cmd1->processId()));
          cmd2->redo();
          disp.submitCommand(cmd2);

          disp.commit();
        });

    dialog.launchWindow();
  });
  menu.addAction(addNewProcessInExistingSlot);

  // Then Add process in a new slot
  auto addNewProcessInNewSlot
      = new QAction{tr("Add process in a new slot"), &menu};
  QObject::connect(addNewProcessInNewSlot, &QAction::triggered, [&]() {
    auto& fact = ctx.app.interfaces<Process::ProcessFactoryList>();
    AddProcessDialog dialog{fact, qApp->activeWindow()};

    QObject::connect(
        &dialog, &AddProcessDialog::okPressed, [&](const auto& proc) {
          using cmd = Scenario::Command::CreateProcessInNewSlot;
          QuietMacroCommandDispatcher<cmd> disp{ctx.commandStack};

          cmd::create(disp, constraint, proc);

          disp.commit();
        });

    dialog.launchWindow();
  });
  menu.addAction(addNewProcessInNewSlot);
}

void ScenarioContextMenuManager::createSlotContextMenu(
    const iscore::DocumentContext& docContext,
    QMenu& menu,
    const FullViewConstraintPresenter& slotp,
    int slot_index)
{

}

void ScenarioContextMenuManager::createLayerContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const Process::LayerContextMenuManager& lcmmgr,
    const Process::LayerPresenter& pres)
{
  using namespace iscore;

  bool has_slot_menu = false;

  // Fill with slot actions
  if (auto full_view = dynamic_cast<FullViewConstraintPresenter*>(pres.parent()))
  {
    auto& context = pres.context().context;
    if (context.selectionStack.currentSelection().toList().isEmpty())
    {
      auto slotSubmenu = menu.addMenu(tr("Slot"));
      ScenarioContextMenuManager::createSlotContextMenu(
          context, *slotSubmenu, *full_view, full_view->indexOfSlot(pres));
      has_slot_menu = true;
    }
  }
  else if(auto small_view = dynamic_cast<TemporalConstraintPresenter*>(pres.parent()))
  {
    auto& context = pres.context().context;
    if (context.selectionStack.currentSelection().toList().isEmpty())
    {
      auto slotSubmenu = menu.addMenu(tr("Slot"));
      ScenarioContextMenuManager::createSlotContextMenu(
          context, *slotSubmenu, *small_view, small_view->indexOfSlot(pres));
      has_slot_menu = true;
    }
  }

  // Then the process-specific part
  if (has_slot_menu)
  {
    auto processMenu = menu.addMenu(pres.layerModel().prettyName());
    pres.fillContextMenu(*processMenu, pos, scenepos, lcmmgr);
  }
  else
  {
    pres.fillContextMenu(menu, pos, scenepos, lcmmgr);
  }
}
}
