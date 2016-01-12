#include "ConstraintActions.hpp"

#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>

#include <core/presenter/MenubarManager.hpp>
#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <iscore/application/ApplicationContext.hpp>

#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Cohesion/InterpolateStates.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedConstraints.hpp>

#include <QAction>
#include <QMenu>
#include <QToolBar>

#include <QApplication>

using namespace iscore;
namespace Scenario
{
ConstraintActions::ConstraintActions(
        iscore::ToplevelMenuElement menuElt,
        ScenarioApplicationPlugin* parent):
    ScenarioActions(menuElt, parent)
{
    const auto& appContext = parent->context;
    auto& fact = appContext.components.factory<Process::ProcessList>();
    m_addProcessDialog = new AddProcessDialog{fact, qApp->activeWindow()};

    connect(m_addProcessDialog, &AddProcessDialog::okPressed,
        this, &ConstraintActions::addProcessInConstraint);

    m_addProcess = new QAction{tr("Add Process in constraint"), this};
    m_addProcess->setWhatsThis(iscore::MenuInterface::name(iscore::ContextMenu::Constraint));
    connect(m_addProcess, &QAction::triggered,
        [this]()
    {
    auto selectedConstraints = selectedElements(m_parent->focusedScenarioModel()->constraints);
    if(selectedConstraints.isEmpty())
        return;
    m_addProcessDialog->launchWindow();
    });

    m_interp = new QAction {tr("Interpolate states"), this};
    m_interp->setShortcutContext(Qt::ApplicationShortcut);
    m_interp->setShortcut(tr("Ctrl+K"));
    m_interp->setToolTip(tr("Ctrl+K"));
    m_interp->setWhatsThis(iscore::MenuInterface::name(iscore::ContextMenu::Constraint));
    connect(m_interp, &QAction::triggered,
        this, [&] () {
    DoForSelectedConstraints(m_parent->currentDocument()->context(), InterpolateStates);
    });

}

void ConstraintActions::fillMenuBar(iscore::MenubarManager* menu)
{
    menu->insertActionIntoToplevelMenu(m_menuElt, m_addProcess);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_interp);
}

void ConstraintActions::fillContextMenu(
    QMenu* menu,
    const Selection& sel,
    const TemporalScenarioPresenter& pres,
    const QPoint&,
    const QPointF&)
{
    if(!sel.empty())
    {
        QList<const ConstraintModel*> selectedConstraints = filterSelectionByType<ConstraintModel>(sel);
        if(selectedConstraints.size() == 1)
        {
            auto& cst = *selectedConstraints.front();
            if(!cst.racks.empty())
            {
                auto rackMenu = menu->addMenu(iscore::MenuInterface::name(iscore::ContextMenu::Rack));

                // We have to find the constraint view model of this layer.
                auto& vm = dynamic_cast<const TemporalScenarioLayerModel*>(&pres.layerModel())->constraint(cst.id());

                for(const RackModel& rack : cst.racks)
                {
                    auto act = new QAction{rack.objectName(), rackMenu};
                    connect(act, &QAction::triggered,
                            this, [&] () {
                        auto cmd = new Scenario::Command::ShowRackInViewModel{vm, rack.id()};
                        CommandDispatcher<> dispatcher{m_parent->currentDocument()->context().commandStack};
                        dispatcher.submitCommand(cmd);
                    });

                    rackMenu->addAction(act);
                }

                auto hideAct = new QAction{tr("Hide"), rackMenu};
                connect(hideAct, &QAction::triggered,
                        this, [&] () {
                    auto cmd = new Scenario::Command::HideRackInViewModel{vm};
                    CommandDispatcher<> dispatcher{m_parent->currentDocument()->context().commandStack};
                    dispatcher.submitCommand(cmd);
                });
                rackMenu->addAction(hideAct);
            }
        }

        if(selectedConstraints.size() >= 1)
        {
            auto cstrSubmenu = menu->findChild<QMenu*>(MenuInterface::name(iscore::ContextMenu::Constraint));
            if(!cstrSubmenu)
            {
                cstrSubmenu = menu->addMenu(MenuInterface::name(iscore::ContextMenu::Constraint));
                cstrSubmenu->setTitle(MenuInterface::name(iscore::ContextMenu::Constraint));
            }

            if(m_addProcess)
                cstrSubmenu->addAction(m_addProcess);
            cstrSubmenu->addAction(m_interp);
        }

    }
}

void ConstraintActions::setEnabled(bool b)
{
    for (auto& act : actions())
    {
    act->setEnabled(b);
    }
}

bool ConstraintActions::populateToolBar(QToolBar* b)
{
    b->addAction(m_interp);
    return true;
}

QList<QAction*> ConstraintActions::actions() const
{
    QList<QAction*> lst{
    m_interp,
    m_addProcess
    };
    return lst;
}

void ConstraintActions::addProcessInConstraint(const ProcessFactoryKey& processName)
{
    auto selectedConstraints = selectedElements(m_parent->focusedScenarioModel()->constraints);
    if(selectedConstraints.isEmpty())
    return;
    auto cmd = Scenario::Command::make_AddProcessToConstraint( //NOTE just the first, not all ?
    **selectedConstraints.begin(),
    processName);

    emit dispatcher().submitCommand(cmd);
}

CommandDispatcher<> ConstraintActions::dispatcher()
{
    CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
    return disp;
}
}
