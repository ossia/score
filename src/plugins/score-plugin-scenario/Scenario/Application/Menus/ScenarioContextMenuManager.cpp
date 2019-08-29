// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "ScenarioContextMenuManager.hpp"

#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Commands/Interval/AddLayerInNewSlot.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/CreateProcessInExistingSlot.hpp>
#include <Scenario/Commands/Interval/CreateProcessInNewSlot.hpp>
#include <Scenario/Commands/Interval/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Interval/Rack/RemoveSlotFromRack.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Interval/Rack/SwapSlots.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateCommentBlock.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>
#include <Scenario/ViewCommands/PutLayerModelToFront.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/std/Optional.hpp>

#include <QAction>
#include <QMenu>
#include <QPoint>

namespace Scenario
{
void ScenarioContextMenuManager::createProcessSelectorContextMenu(
    const score::DocumentContext& ctx,
    QMenu& menu,
    const TemporalIntervalPresenter& pres,
    int slot_index)
{
  using namespace Scenario::Command;
  // TODO see
  // http://stackoverflow.com/questions/21443023/capturing-a-reference-by-reference-in-a-c11-lambda
  auto& interval = pres.model();
  const Slot& slot = interval.smallView().at(slot_index);
  SlotPath slot_path{interval, slot_index, Slot::SmallView};

  for (const Id<Process::ProcessModel>& proc : slot.processes)
  {
    auto& p = interval.processes.at(proc);
    auto name = p.prettyName();
    if (name.isEmpty())
      name = p.prettyShortName();
    QAction* procAct = new QAction{name, &menu};
    QObject::connect(procAct, &QAction::triggered, [&, slot_path]() mutable {
      PutLayerModelToFront cmd{std::move(slot_path), p.id()};
      cmd.redo(ctx);
    });
    menu.addAction(procAct);
  }
}
}
