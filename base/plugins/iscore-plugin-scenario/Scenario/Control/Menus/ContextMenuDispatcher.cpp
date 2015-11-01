#include "ContextMenuDispatcher.hpp"
#include <Scenario/Control/ScenarioControl.hpp>
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

#include <Scenario/DialogWidget/AddProcessDialog.hpp>

#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <Scenario/ViewCommands/PutLayerModelToFront.hpp>

#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Control/Menus/ScenarioActions.hpp>

#include <QMenu>

void ScenarioContextMenuManager::createSlotContextMenu(QMenu& menu, const SlotPresenter& slotp)
{
    auto& slotm = slotp.model();

    // First changing the process in the current slot
    auto processes_submenu = menu.addMenu(tr("Focus process in slot"));
    for(const LayerModel& proc : slotm.layers)
    {
        QAction* procAct = new QAction{
                           proc.processModel().userFriendlyDescription(),
                           processes_submenu};
        connect(procAct, &QAction::triggered, this, [&] () {
            PutLayerModelToFront cmd{slotm, proc.id()};
            cmd.redo();
        } );
        processes_submenu->addAction(procAct);
    }

    // Then creation of a new slot with existing processes

    // Then removal of slot
    auto removeSlotAct = new QAction{tr("Remove this slot"), nullptr};
    connect(removeSlotAct, &QAction::triggered,
            this, [&] () {
        auto cmd = new Scenario::Command::RemoveSlotFromRack{slotm};
        CommandDispatcher<>{m_control.currentDocument()->commandStack()}.submitCommand(cmd);
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
            QAction* procAct = new QAction{proc.userFriendlyDescription(), existing_processes_submenu};
            connect(procAct, &QAction::triggered, this, [&] () {

                auto cmd2 = new Scenario::Command::AddLayerModelToSlot{
                            slotm,
                            proc};
                CommandDispatcher<>{m_control.currentDocument()->commandStack()}.submitCommand(cmd2);
            } );
            existing_processes_submenu->addAction(procAct);
        }
    }

    auto addNewProcessInExistingSlot = new QAction{tr("Add new process in this slot"), &menu};
    connect(addNewProcessInExistingSlot, &QAction::triggered,
            this, [&] () {
        AddProcessDialog dialog(qApp->activeWindow());

        con(dialog, &AddProcessDialog::okPressed,
            this, [&] (const QString& proc) {
            auto& constraint = slotm.parentConstraint();
            QuietMacroCommandDispatcher disp{
                new CreateProcessInExistingSlot,
                        m_control.currentDocument()->commandStack()};

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
    connect(addNewProcessInNewSlot, &QAction::triggered,
            this, [&] () {
        AddProcessDialog dialog(qApp->activeWindow());

        con(dialog, &AddProcessDialog::okPressed,
            this, [&] (const QString& proc) {
            auto& constraint = slotm.parentConstraint();
            QuietMacroCommandDispatcher disp{
                new CreateProcessInNewSlot,
                        m_control.currentDocument()->commandStack()};

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
        createSlotContextMenu(menu, *slotp);
    }

    // Then the process-specific part
    pres.fillContextMenu(&menu, pos, scenepos);
}

void ScenarioContextMenuManager::createScenarioContextMenu(
        QMenu& menu,
        const QPoint& pos,
        const QPointF& scenepos,
        const TemporalScenarioPresenter& pres)
{
    auto selected = pres.layerModel().processModel().selectedChildren();

    for(ScenarioActions*& elt : m_control.m_pluginActions)
    {
        // TODO make a class to encapsulate all the data
        // required to set-up a context menu in a scenario.
        elt->fillContextMenu(&menu, selected, pres, pos, scenepos);
        menu.addSeparator();
    }

    menu.addSeparator();
    menu.addAction(m_control.m_selectAll);
    menu.addAction(m_control.m_deselectAll);

}
