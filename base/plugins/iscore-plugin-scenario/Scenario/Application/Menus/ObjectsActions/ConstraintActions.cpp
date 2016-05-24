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
#include <iscore/widgets/SetIcons.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/Commands/Constraint/AddProcessToConstraint.hpp>
#include <Scenario/Commands/Cohesion/InterpolateStates.hpp>
#include <Scenario/Commands/Cohesion/DoForSelectedConstraints.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <QAction>
#include <QMenu>
#include <QToolBar>

#include <QApplication>

namespace Scenario
{
// TODO you're better than this
auto selectedConstraintsInCurrentDocument(const iscore::ApplicationContext& appContext);
auto selectedConstraintsInCurrentDocument(const iscore::ApplicationContext& appContext)
{
    auto sel = appContext.documents.currentDocument()->selectionStack().currentSelection();
    QList<const Scenario::ConstraintModel*> selected_elements;
    for(auto obj : sel)
    {
        if(auto casted_obj = dynamic_cast<const Scenario::ConstraintModel*>(obj.data()))
        {
            selected_elements.push_back(casted_obj);
        }
    }

    return selected_elements;
}

ConstraintActions::ConstraintActions(
        ScenarioApplicationPlugin* parent) :
    m_parent{parent}
{
    const auto& appContext = parent->context;
    auto& fact = appContext.components.factory<Process::ProcessList>();
    m_addProcessDialog = new AddProcessDialog{fact, qApp->activeWindow()};

    connect(m_addProcessDialog, &AddProcessDialog::okPressed,
        this, &ConstraintActions::addProcessInConstraint);

    m_addProcess = new QAction{tr("Add Process in constraint"), this};
    m_addProcess->setWhatsThis(iscore::MenuInterface::name(iscore::ContextMenu::Constraint));
    connect(m_addProcess, &QAction::triggered,
            [&]()
    {
        if(selectedConstraintsInCurrentDocument(appContext).isEmpty())
            return;
        m_addProcessDialog->launchWindow();
    });

    m_interp = new QAction {tr("Interpolate states"), this};
    m_interp->setShortcutContext(Qt::ApplicationShortcut);
    m_interp->setShortcut(tr("Ctrl+K"));
    m_interp->setToolTip(tr("Interpolate states (Ctrl+K)"));
    m_interp->setWhatsThis(iscore::MenuInterface::name(iscore::ContextMenu::Constraint));
    setIcons(m_interp, QString(":/icons/interpolate_on.png"), QString(":/icons/interpolate_off.png"));
    connect(m_interp, &QAction::triggered,
        this, [&] () {
    DoForSelectedConstraints(m_parent->currentDocument()->context(), Command::InterpolateStates);
    });

}

ConstraintActions::~ConstraintActions()
{
    delete m_addProcessDialog;
}

void ConstraintActions::makeGUIElements(iscore::GUIElements& ref)
{
    using namespace iscore;
    auto& scenario_iface_cond = m_parent->context.actions.condition<Process::EnableWhenFocusedProcessIs<Scenario::ScenarioInterface>>();

    Menu& object = m_parent->context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_addProcess);
    object.menu()->addAction(m_interp);
    {
        auto bar = new QToolBar{tr("Constraint")};
        bar->addAction(m_interp);
        ref.toolbars.emplace_back(bar, StringKey<iscore::Toolbar>("Constraint"), 0, 0);
    }

    ref.actions.add<Actions::AddProcess>(m_addProcess);
    ref.actions.add<Actions::InterpolateStates>(m_interp);

    scenario_iface_cond.add<Actions::AddProcess>();
    scenario_iface_cond.add<Actions::InterpolateStates>();
}

void ConstraintActions::fillContextMenu(
    QMenu* menu,
    const Selection& sel,
    const TemporalScenarioPresenter& pres,
    const QPoint&,
    const QPointF&)
{
    using namespace iscore;
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
                    auto act = new QAction{rack.metadata.name(), rackMenu};
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
            {
                cstrSubmenu->addAction(m_addProcess);
            }
            cstrSubmenu->addAction(m_interp);
        }

    }
}

void ConstraintActions::fillContextMenu(
        QMenu* menu,
        const Selection&,
        const ConstraintViewModel& vm,
        const QPoint&,
        const QPointF&)
{
    using namespace iscore;
    auto& cst = vm.model();
    if(!cst.racks.empty())
    {
        auto rackMenu = menu->addMenu(iscore::MenuInterface::name(iscore::ContextMenu::Rack));

        for(const RackModel& rack : cst.racks)
        {
            auto act = new QAction{rack.metadata.name(), rackMenu};
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

    auto cstrSubmenu = menu->findChild<QMenu*>(MenuInterface::name(iscore::ContextMenu::Constraint));
    if(!cstrSubmenu)
    {
        cstrSubmenu = menu->addMenu(MenuInterface::name(iscore::ContextMenu::Constraint));
        cstrSubmenu->setTitle(MenuInterface::name(iscore::ContextMenu::Constraint));
    }

    if(m_addProcess)
    {
        cstrSubmenu->addAction(m_addProcess);
    }
    cstrSubmenu->addAction(m_interp);
}

void ConstraintActions::addProcessInConstraint(const UuidKey<Process::ProcessFactory>& processName)
{
    auto selectedConstraints = selectedConstraintsInCurrentDocument(m_parent->context);
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
