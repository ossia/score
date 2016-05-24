#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
// This part is somewhat similar to what moc does
// with moc_.. stuff generation.
#include <QAction>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeySequence>
#include <QList>
#include <QMenu>
#include <qnamespace.h>
#include <QString>
#include <QToolBar>
#include <string.h>

#include "Menus/TransportActions.hpp"
#include <Process/LayerModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/Process.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Application/Menus/ObjectMenuActions.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include <Process/Tools/ProcessGraphicsView.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Palette/Tool.hpp>
#include "ScenarioApplicationPlugin.hpp"
#include <core/document/Document.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/document/DocumentView.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModelInterface.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
#include <Scenario/Application/Menus/ScenarioCommonContextMenuFactory.hpp>
#include <algorithm>
#include <core/presenter/Presenter.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/view/View.hpp>
#include <Scenario/Application/ScenarioActions.hpp>

namespace Scenario
{
void test_parse_expr_full();

ScenarioApplicationPlugin::ScenarioApplicationPlugin(const iscore::ApplicationContext& ctx) :
    GUIApplicationContextPlugin{ctx}
{
    connect(qApp, &QApplication::applicationStateChanged,
            this, [&] (Qt::ApplicationState st) {
        editionSettings().setDefault();
    });

    using namespace iscore;
    using namespace Scenario;
    using namespace Process;
    ctx.actions.onFocusChange(std::make_shared<EnableWhenFocusedObjectIs<TemporalScenarioLayerModel>>());
    ctx.actions.onFocusChange(std::make_shared<EnableWhenFocusedProcessIs<ScenarioModel>>());
    ctx.actions.onFocusChange(std::make_shared<EnableWhenFocusedProcessIs<ScenarioInterface>>());
    ctx.actions.onDocumentChange(std::make_shared<EnableWhenDocumentIs<ScenarioDocumentModel>>());

    ctx.actions.onSelectionChange(std::make_shared<EnableWhenSelectionContains<ConstraintModel>>());
    ctx.actions.onSelectionChange(std::make_shared<EnableWhenSelectionContains<EventModel>>());
    ctx.actions.onSelectionChange(std::make_shared<EnableWhenSelectionContains<StateModel>>());

    // TODO : enable when focus on scenario && not empty
    auto on_sm = std::make_shared<EnableWhenScenarioModelObject>();
    ctx.actions.onSelectionChange(on_sm);
    ctx.actions.onFocusChange(on_sm);
    auto on_si = std::make_shared<EnableWhenScenarioInterfaceObject>();
    ctx.actions.onSelectionChange(on_si);
    ctx.actions.onFocusChange(on_si);
}

auto ScenarioApplicationPlugin::makeGUIElements() -> GUIElements
{
    using namespace iscore;
    GUIElements e;

    {
        m_selectAll = new QAction{tr("Select all"), this};
        m_selectAll->setToolTip("Ctrl+a");
        connect(m_selectAll, &QAction::triggered,
                [this]()
        {
            auto doc = currentDocument();
            if(!doc)
                return;

            auto pres = IDocument::try_get<ScenarioDocumentPresenter>(*doc);
            if(pres)
                pres->selectAll();
        });

        m_deselectAll = new QAction{tr("Deselect all"), this};
        m_deselectAll->setToolTip("Ctrl+Shift+a");
        connect(m_deselectAll, &QAction::triggered,
                [this]()
        {
            auto doc = currentDocument();
            if(!doc)
                return;

            auto pres = IDocument::try_get<ScenarioDocumentPresenter>(*doc);
            if(pres)
                pres->deselectAll();
        });


        Menu& menu = context.menus.get().at(Menus::View());
        menu.menu()->addAction(m_selectAll);
        menu.menu()->addAction(m_deselectAll);

        e.actions.add<Actions::SelectAll>(m_selectAll);
        e.actions.add<Actions::DeselectAll>(m_deselectAll);

        auto& cond = context.actions.condition<iscore::EnableWhenDocumentIs<Scenario::ScenarioDocumentModel>>();
        cond.add<Actions::SelectAll>();
        cond.add<Actions::DeselectAll>();
    }

    m_objectActions.makeGUIElements(e);
    m_toolActions.makeGUIElements(e);
    m_transportActions.makeGUIElements(e);

    return e;
}

ScenarioApplicationPlugin::~ScenarioApplicationPlugin() = default;

void ScenarioApplicationPlugin::on_presenterDefocused(Process::LayerPresenter* pres)
{
    // We set the currently focused view model to a "select" state
    // to prevent problems.
    editionSettings().setDefault();

    // TODO Actions
    /*
    for(ScenarioActions*& elt : m_pluginActions)
    {
        elt->setEnabled(false);
    }
    */

    disconnect(m_contextMenuConnection);
}


void ScenarioApplicationPlugin::on_presenterFocused(Process::LayerPresenter* pres)
{
    // Generic stuff
    if(focusedPresenter())
    {
        disconnect(m_contextMenuConnection);
    }
    if(pres)
    {
        m_contextMenuConnection = connect(pres, &Process::LayerPresenter::contextMenuRequested,
                this, [=] (const QPoint& pos, const QPointF& pt2) {
            QMenu menu(qApp->activeWindow());
            ScenarioContextMenuManager::createLayerContextMenu(menu, pos, pt2, *pres);
            menu.exec(pos);
            menu.close();
        } );
    }
    else
    {
        return;
    }

    auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres);

    if(s_pres)
    {
        context.actions.action<Actions::SelectTool>().action()->setEnabled(true);
        context.actions.action<Actions::CreateTool>().action()->setEnabled(true);
        context.actions.action<Actions::PlayTool>().action()->setEnabled(true);
    }
    // TODO Actions
    /*
    // Case specific to the scenario process.
    // First get the scenario presenter
    auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres);

    for(ScenarioActions* elt : m_pluginActions)
    {
        if(dynamic_cast<ObjectMenuActions*>(elt))
            elt->setEnabled(false);
        else
            elt->setEnabled(bool(s_pres));
    }
    */

    if (s_pres)
    {
        connect(s_pres, &TemporalScenarioPresenter::keyPressed,
                this,  &ScenarioApplicationPlugin::keyPressed);

        connect(s_pres, &TemporalScenarioPresenter::keyReleased,
                this,  &ScenarioApplicationPlugin::keyReleased);

        auto& select_act = context.actions.action<Actions::SelectTool>();
        select_act.action()->trigger();
    }
}

void ScenarioApplicationPlugin::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
{
    // TODO Actions
    using namespace iscore;
    // TODO the context menu connection should be reviewed, too.
    this->disconnect(m_focusConnection);
    this->disconnect(m_defocusConnection);

    m_editionSettings.setDefault();
    m_editionSettings.setExecution(false);
    if(!newdoc)
    {
        return;
    }
    else
    {
        auto focusManager = processFocusManager();

        if(!focusManager)
            return;

        m_focusConnection =
                connect(focusManager, &Process::ProcessFocusManager::sig_focusedPresenter,
                        this, &ScenarioApplicationPlugin::on_presenterFocused);
        m_defocusConnection =
                connect(focusManager, &Process::ProcessFocusManager::sig_defocusedPresenter,
                        this, &ScenarioApplicationPlugin::on_presenterDefocused);

        if(focusManager->focusedPresenter())
        {
            // Used when switching between documents
            on_presenterFocused(focusManager->focusedPresenter());
        }
        else
        {
            // We focus by default the first process of the constraint in full view we're in
            // TODO this snippet is useful, put it somewhere in some Toolkit file.
            auto& pres = IDocument::presenterDelegate<ScenarioDocumentPresenter>(*newdoc);
            auto& cst = pres.displayedConstraint();
            auto cst_pres = pres.presenters().constraintPresenter();

            if(!cst.processes.empty() && cst_pres && cst_pres->rack())
            {
                auto rack = cst_pres->rack();
                const auto& slts = rack->getSlots();
                if(!slts.empty())
                {
                    const auto& top_slot = rack->model().slotsPositions().front();
                    const SlotPresenter& first = slts.at(top_slot);
                    const auto& slot_processes = first.processes();
                    if(!slot_processes.empty())
                    {
                        const auto& front_proc = *first.model().frontLayerModel(); // Won't crash because not empty
                        auto it = std::find_if(slot_processes.begin(), slot_processes.end(),
                                               [&] (const auto& proc_elt) {
                            return proc_elt.model == &front_proc;
                        });
                        if(it != slot_processes.end())
                        {
                            const SlotProcessData& proc_elt = *it;
                            if(!proc_elt.processes.empty())
                            {
                                focusManager->focus(proc_elt.processes.front().first);
                            }
                        }
                    }
                }
            }
        }

        // Finally we focus the View widget.
        auto bev = dynamic_cast<ScenarioDocumentView*>(&newdoc->view().viewDelegate());
        if(bev)
            bev->view().setFocus();
    }
}

void ScenarioApplicationPlugin::on_activeWindowChanged()
{
    editionSettings().setDefault(); // NOTE maybe useless now ?
}

const Scenario::ScenarioModel* ScenarioApplicationPlugin::focusedScenarioModel() const
{
    if(auto focusManager = processFocusManager())
    {
        return dynamic_cast<const Scenario::ScenarioModel*>(focusManager->focusedModel());
    }
    return nullptr;
}

const Scenario::ScenarioInterface* ScenarioApplicationPlugin::focusedScenarioInterface() const
{
    if(auto focusManager = processFocusManager())
    {
        return dynamic_cast<const Scenario::ScenarioInterface*>(focusManager->focusedModel());
    }
    return nullptr;
}

TemporalScenarioPresenter* ScenarioApplicationPlugin::focusedPresenter() const
{
    if(auto focusManager = processFocusManager())
    {
        return dynamic_cast<TemporalScenarioPresenter*>(focusManager->focusedPresenter());
    }
    return nullptr;
}

void ScenarioApplicationPlugin::prepareNewDocument()
{
    auto& stop_action = context.actions.action<Actions::Stop>();
    stop_action.action()->trigger();
}

Process::ProcessFocusManager* ScenarioApplicationPlugin::processFocusManager() const
{
    if(auto doc = currentDocument())
    {
        auto bem = dynamic_cast<ScenarioDocumentModel*>(&doc->model().modelDelegate());
        if(bem)
        {
            return &bem->focusManager();
        }
    }

    return nullptr;
}
}

