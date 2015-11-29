#include "ScenarioContextMenuManager.hpp"
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>

#include <Scenario/Commands/Constraint/Rack/RemoveSlotFromRack.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/CreateProcessInExistingSlot.hpp>
#include <Scenario/Commands/Constraint/CreateProcessInNewSlot.hpp>
#include <Scenario/Commands/Constraint/AddLayerInNewSlot.hpp>

#include <Scenario/DialogWidget/AddProcessDialog.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <Scenario/ViewCommands/PutLayerModelToFront.hpp>

#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <QApplication>

#include <QMenu>

void ScenarioContextMenuManager::createSlotContextMenu(
        const iscore::DocumentContext& ctx,
        QMenu& menu,
        const SlotPresenter& slotp)
{
    auto& slotm = slotp.model();

    // First changing the process in the current slot
    auto processes_submenu = menu.addMenu(tr("Focus process in this slot"));
    for(const LayerModel& proc : slotm.layers)
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
    for(const LayerModel& proc : slotm.layers)
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
    for(const Process& proc : slotm.parentConstraint().processes)
    {
        // OPTIMIZEME by filtering before.
        if(std::none_of(slotm.layers.begin(), slotm.layers.end(), [&] (const LayerModel& layer) {
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
        auto& fact = ctx.app.components.factory<DynamicProcessList>();
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
        auto& fact = ctx.app.components.factory<DynamicProcessList>();
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

    menu.addSeparator();
}

void ScenarioContextMenuManager::createLayerContextMenu(
        QMenu& menu,
        const QPoint& pos,
        const QPointF& scenepos,
        const LayerPresenter& pres)
{
    // Fill with slot actions
    if(auto slotp = dynamic_cast<SlotPresenter*>(pres.parent()))
    {
        auto& context = iscore::IDocument::documentContext(slotp->model());
        ScenarioContextMenuManager::createSlotContextMenu(context, menu, *slotp);
    }

    // Then the process-specific part
    pres.fillContextMenu(&menu, pos, scenepos);
}

void ScenarioContextMenuManager::createScenarioContextMenu(
        const iscore::DocumentContext& ctx,
        QMenu& menu,
        const QPoint& pos,
        const QPointF& scenepos,
        const TemporalScenarioPresenter& pres)
{
    auto selected = pres.layerModel().processModel().selectedChildren();

    auto& appPlug = ctx.app.components.applicationPlugin<ScenarioApplicationPlugin>();
    for(auto elt : appPlug.pluginActions())
    {
        // TODO make a class to encapsulate all the data
        // required to set-up a context menu in a scenario.
        elt->fillContextMenu(&menu, selected, pres, pos, scenepos);
        menu.addSeparator();
    }

    menu.addSeparator();
    menu.addAction(appPlug.m_selectAll);
    menu.addAction(appPlug.m_deselectAll);
}
