#include <Process/LayerModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
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
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/ViewCommands/PutLayerModelToFront.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/selection/Selection.hpp>
#include <QAction>
#include <QApplication>
#include <QMenu>

#include <QPoint>
#include <QString>
#include <algorithm>

#include <Process/ProcessList.hpp>
#include "ScenarioContextMenuManager.hpp"

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
void ScenarioContextMenuManager::createSlotContextMenu(
        const iscore::DocumentContext& ctx,
        QMenu& menu,
        const SlotPresenter& slotp)
{
    using namespace Scenario::Command;
    // TODO see http://stackoverflow.com/questions/21443023/capturing-a-reference-by-reference-in-a-c11-lambda
    auto& slotm = slotp.model();

    // First changing the process in the current slot
    auto processes_submenu = menu.addMenu(tr("Focus process in this slot"));
    for(const Process::LayerModel& proc : slotm.layers)
    {
        QAction* procAct = new QAction{
                           proc.processModel().prettyName(),
                           processes_submenu};
        QObject::connect(procAct, &QAction::triggered, [&] () {
            PutLayerModelToFront cmd{slotm, proc.id()};
            cmd.redo();
        } );
        processes_submenu->addAction(procAct);
    }

    // Then creation of a new slot with existing processes
    auto new_processes_submenu = menu.addMenu(tr("Show process in new slot"));
    for(const Process::LayerModel& proc : slotm.layers)
    {
        QAction* procAct = new QAction{
                           proc.processModel().prettyName(),
                           new_processes_submenu};
        QObject::connect(procAct, &QAction::triggered, [&] () {
            auto cmd = new Scenario::Command::AddLayerInNewSlot{
                       slotm.parentConstraint(),
                       proc.processModel().id()};
            CommandDispatcher<>{ctx.commandStack}.submitCommand(cmd);
        } );
        new_processes_submenu->addAction(procAct);
    }

    // Then removal of slot
    auto removeSlotAct = new QAction{tr("Remove this slot"), nullptr};
    QObject::connect(removeSlotAct, &QAction::triggered, [&] () {
        auto cmd = new Scenario::Command::RemoveSlotFromRack{slotm};
        CommandDispatcher<>{ctx.commandStack}.submitCommand(cmd);
    });
    menu.addAction(removeSlotAct);

    menu.addSeparator();

    // Then Add process in this slot
    auto existing_processes_submenu = menu.addMenu(tr("Add existing process in this slot"));
    for(const Process::ProcessModel& proc : slotm.parentConstraint().processes)
    {
        // OPTIMIZEME by filtering before.
        if(std::none_of(slotm.layers.begin(), slotm.layers.end(), [&] (const Process::LayerModel& layer) {
                        return &layer.processModel() == &proc;
    }))
        {
            QAction* procAct = new QAction{proc.prettyName(), existing_processes_submenu};
            QObject::connect(procAct, &QAction::triggered, [&] () {

                auto cmd2 = new Scenario::Command::AddLayerModelToSlot{
                            slotm,
                            proc};
                CommandDispatcher<>{ctx.commandStack}.submitCommand(cmd2);
            } );
            existing_processes_submenu->addAction(procAct);
        }
    }

    auto addNewProcessInExistingSlot = new QAction{tr("Add new process in this slot"), &menu};
    QObject::connect(addNewProcessInExistingSlot, &QAction::triggered,
            [&] () {
        auto& fact = ctx.app.components.factory<Process::ProcessList>();
        AddProcessDialog dialog{fact, qApp->activeWindow()};

        QObject::connect(&dialog, &AddProcessDialog::okPressed,
            [&] (const auto& proc) {
            auto& constraint = slotm.parentConstraint();
            QuietMacroCommandDispatcher disp{
                new CreateProcessInExistingSlot,
                        ctx.commandStack};

            auto cmd1 = new AddOnlyProcessToConstraint{constraint, proc};
            cmd1->redo();
            disp.submitCommand(cmd1);

            auto cmd2 = new Scenario::Command::AddLayerModelToSlot{
                        slotm,
                        constraint.processes.at(cmd1->processId())};
            cmd2->redo();
            disp.submitCommand(cmd2);

            disp.commit();
        });

        dialog.launchWindow();
    });
    menu.addAction(addNewProcessInExistingSlot);

    // Then Add process in a new slot
    auto addNewProcessInNewSlot = new QAction{tr("Add process in a new slot"), &menu};
    QObject::connect(addNewProcessInNewSlot, &QAction::triggered,
            [&] () {
        auto& fact = ctx.app.components.factory<Process::ProcessList>();
        AddProcessDialog dialog{fact, qApp->activeWindow()};

        QObject::connect(&dialog, &AddProcessDialog::okPressed,
            [&] (const auto& proc) {
            auto& constraint = slotm.parentConstraint();
            QuietMacroCommandDispatcher disp{
                new CreateProcessInNewSlot,
                        ctx.commandStack};

            auto cmd1 = new AddOnlyProcessToConstraint{constraint, proc};
            cmd1->redo();
            disp.submitCommand(cmd1);

            auto& rack = slotm.rack();
            auto cmd2 = new Scenario::Command::AddSlotToRack{rack};
            cmd2->redo();
            disp.submitCommand(cmd2);

            auto cmd3 = new Scenario::Command::AddLayerModelToSlot{
                        rack.slotmodels.at(cmd2->createdSlot()),
                        constraint.processes.at(cmd1->processId())};
            cmd3->redo();
            disp.submitCommand(cmd3);

            disp.commit();
        });

        dialog.launchWindow();
    });
    menu.addAction(addNewProcessInNewSlot);
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
    if(auto slotp = dynamic_cast<SlotPresenter*>(pres.parent()))
    {
        auto& context = pres.context().context;
        if (context.selectionStack.currentSelection().toList().isEmpty())
        {
            auto slotSubmenu = menu.addMenu(tr("Slot"));
            ScenarioContextMenuManager::createSlotContextMenu(context, *slotSubmenu, *slotp);
            has_slot_menu = true;
        }
    }

    // Then the process-specific part
    if(has_slot_menu)
    {
        auto processMenu = menu.addMenu(tr("Process"));
        pres.fillContextMenu(*processMenu, pos, scenepos, lcmmgr);
    }
    else
    {
        pres.fillContextMenu(menu, pos, scenepos, lcmmgr);
    }
}
}
