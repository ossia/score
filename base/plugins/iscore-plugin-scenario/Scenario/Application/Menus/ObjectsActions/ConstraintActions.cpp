#include "ConstraintActions.hpp"

#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>


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
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <QAction>
#include <QMenu>
#include <QToolBar>
#include <QMainWindow>
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
    connect(m_addProcess, &QAction::triggered,
            [&]()
    {
        if(selectedConstraintsInCurrentDocument(appContext).isEmpty())
            return;
        m_addProcessDialog->launchWindow();
    });

    m_interp = new QAction {tr("Interpolate states"), this};
    m_interp->setShortcutContext(Qt::ApplicationShortcut);
    m_interp->setToolTip(tr("Interpolate states (Ctrl+K)"));
    setIcons(m_interp, QString(":/icons/interpolate_on.png"), QString(":/icons/interpolate_off.png"));
    connect(m_interp, &QAction::triggered,
        this, [&] () {
    DoForSelectedConstraints(m_parent->currentDocument()->context(), Command::InterpolateStates);
    });


    m_curves = new QAction{this};
    m_curves->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    // TODO add "center widget" and "panels"
    parent->context.mainWindow.addAction(m_curves);

    setIcons(m_curves, QString(":/icons/create_curve_on.png"), QString(":/icons/create_curve_off.png"));

    connect(m_curves, &QAction::triggered,
            this, [&] () {
        if(auto doc = m_parent->currentDocument())
            DoForSelectedConstraints(doc->context(), CreateCurves);
    });
    m_curves->setEnabled(false);

}

ConstraintActions::~ConstraintActions()
{
    delete m_addProcessDialog;
}

void ConstraintActions::makeGUIElements(iscore::GUIElements& ref)
{
    using namespace iscore;


    Menu& object = m_parent->context.menus.get().at(Menus::Object());
    object.menu()->addAction(m_addProcess);
    object.menu()->addAction(m_interp);
    object.menu()->addAction(m_curves);
    {
        auto bar = new QToolBar{tr("Constraint")};
        bar->addAction(m_interp);
        bar->addAction(m_curves);
        ref.toolbars.emplace_back(bar, StringKey<iscore::Toolbar>("Constraint"), 0, 0);
    }

    ref.actions.add<Actions::AddProcess>(m_addProcess);
    ref.actions.add<Actions::InterpolateStates>(m_interp);
    ref.actions.add<Actions::CreateCurves>(m_curves);

    auto& cond = m_parent->context.actions.condition<iscore::EnableWhenSelectionContains<Scenario::ConstraintModel>>();
    cond.add<Actions::AddProcess>();
    cond.add<Actions::InterpolateStates>();
    cond.add<Actions::CreateCurves>();
}

void ConstraintActions::setupContextMenu(Process::LayerContextMenuManager &ctxm)
{
    using namespace Process;
    Process::LayerContextMenu cm = MetaContextMenu<ContextMenus::ConstraintContextMenu>::make();

    cm.functions.push_back(
    [this] (QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx)
    {
        using namespace iscore;
        auto sel = ctx.context.selectionStack.currentSelection();
        if(sel.empty())
            return;

        QList<const ConstraintModel*> selectedConstraints = filterSelectionByType<ConstraintModel>(sel);
        if(selectedConstraints.size() == 1)
        {
            auto& cst = *selectedConstraints.front();
            if(!cst.racks.empty())
            {
                auto rackMenu = menu.addMenu(tr("Racks"));

                // We have to find the constraint view model of this layer.
                auto& vm = dynamic_cast<const TemporalScenarioLayer*>(&ctx.layerPresenter.layerModel())->constraint(cst.id());

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
            auto cstrSubmenu = menu.addMenu(tr("Constraint"));
            cstrSubmenu->setObjectName("Constraint");
            if(m_addProcess)
            {
                cstrSubmenu->addAction(m_addProcess);
            }
            cstrSubmenu->addAction(m_interp);
        }
    });

    ctxm.insert(std::move(cm));
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
