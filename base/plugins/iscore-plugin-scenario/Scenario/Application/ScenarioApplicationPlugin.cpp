#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <boost/optional/optional.hpp>

#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
// This part is somewhat similar to what moc does
// with moc_.. stuff generation.
#include <QAction>
#include <QByteArray>
#include <QColor>
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
#include <Scenario/Document/ScenarioDocument/Widgets/ScenarioBaseGraphicsView.hpp>
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
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>

class QPoint;
class QPointF;
namespace iscore {

}  // namespace iscore

using namespace iscore;
#include <Scenario/Application/Menus/ScenarioCommonContextMenuFactory.hpp>
#include <algorithm>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>

void test_parse_expr_full();
ScenarioApplicationPlugin::ScenarioApplicationPlugin(const iscore::ApplicationContext& ctx) :
    GUIApplicationContextPlugin{ctx, "ScenarioApplicationPlugin", nullptr}
{
    connect(qApp, &QApplication::applicationStateChanged,
            this, [&] (Qt::ApplicationState st) {
        editionSettings().setDefault();
    });

    // Note : they are constructed here, because
    // they need to be available quickly for other plug-ins,
    // not after factory loading.
    auto fact  = new ScenarioCommonActionsFactory;
    for(const auto& act : fact->make(this))
    {
        m_pluginActions.push_back(act);
    }
    delete fact;

    initColors();
}

void ScenarioApplicationPlugin::populateMenus(iscore::MenubarManager *menu)
{
    ///// Edit /////

    ///// View /////
    // TODO create ViewMenuActions
    m_selectAll = new QAction{tr("Select all"), this};
    m_selectAll->setShortcut(QKeySequence::SelectAll);
    m_selectAll->setToolTip("Ctrl+a");
    connect(m_selectAll, &QAction::triggered,
            [this]()
    {
        auto &pres = IDocument::presenterDelegate<ScenarioDocumentPresenter>(*currentDocument());
        pres.selectAll();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       m_selectAll);


    m_deselectAll = new QAction{tr("Deselect all"), this};
    m_deselectAll->setShortcut(QKeySequence::Deselect);
    m_deselectAll->setToolTip("Ctrl+Shift+a");
    connect(m_deselectAll, &QAction::triggered,
            [this]()
    {
        auto &pres = IDocument::presenterDelegate<ScenarioDocumentPresenter>(*currentDocument());
        pres.deselectAll();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       m_deselectAll);


    for(ScenarioActions*& elt : m_pluginActions)
    {
        elt->fillMenuBar(menu);
    }
}

std::vector<OrderedToolbar> ScenarioApplicationPlugin::makeToolbars()
{
    QToolBar *bar = new QToolBar;

    int i = 0;
    for(const auto& act : m_pluginActions)
    {
        if(dynamic_cast<TransportActions*>(act))
            continue;

        if(act->populateToolBar(bar))
        {
            if(i < m_pluginActions.size() - 1)
                bar->addSeparator();
        }

        i++;
    }


    return std::vector<OrderedToolbar>{OrderedToolbar(1, bar)};
}

std::vector<QAction*> ScenarioApplicationPlugin::actions()
{
    // TODO add the others
    std::vector<QAction*> act;
    for(const auto& elt : m_pluginActions)
    {
        auto actions = elt->actions();
        act.insert(act.end(), actions.begin(), actions.end());
    }
    return act;
}

void ScenarioApplicationPlugin::on_presenterDefocused(LayerPresenter* pres)
{
    // We set the currently focused view model to a "select" state
    // to prevent problems.
    editionSettings().setDefault();

    for(ScenarioActions*& elt : m_pluginActions)
    {
        elt->setEnabled(false);
    }

    disconnect(m_contextMenuConnection);
}


void ScenarioApplicationPlugin::on_presenterFocused(LayerPresenter* pres)
{
    // Generic stuff
    if(focusedPresenter())
    {
        disconnect(m_contextMenuConnection);
    }
    if(pres)
    {
        m_contextMenuConnection = connect(pres, &LayerPresenter::contextMenuRequested,
                this, [=] (const QPoint& pos, const QPointF& pt2) {
            QMenu menu;
            ScenarioContextMenuManager::createLayerContextMenu(menu, pos, pt2, *pres);
            menu.exec(pos);
            menu.close();
        } );
    }


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

    if (s_pres)
    {
        connect(s_pres, &TemporalScenarioPresenter::keyPressed,
                this,  &ScenarioApplicationPlugin::keyPressed);

        connect(s_pres, &TemporalScenarioPresenter::keyReleased,
                this,  &ScenarioApplicationPlugin::keyReleased);


        for(ScenarioActions* elt : m_pluginActions)
        {
            const auto& acts = elt->actions();
            auto it = std::find_if(acts.begin(), acts.end(), [] (const auto& act) { return act->objectName() == "Select"; });
            if(it != acts.end())
            {
                (*it)->trigger();
                break;
            }
        }
    }
}

void ScenarioApplicationPlugin::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
{
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
                connect(focusManager, &ProcessFocusManager::sig_focusedPresenter,
                        this, &ScenarioApplicationPlugin::on_presenterFocused);
        m_defocusConnection =
                connect(focusManager, &ProcessFocusManager::sig_defocusedPresenter,
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
    editionSettings().setDefault();
}

void ScenarioApplicationPlugin::initColors()
{
    ScenarioStyle& instance = ScenarioStyle::instance();
#ifdef ISCORE_IEEE_SKIN
    QFile cols(":/ScenarioColors-IEEE.json");
#else
    QFile cols(":/ScenarioColors.json");
#endif
    if(cols.open(QFile::ReadOnly))
    {
        auto obj = QJsonDocument::fromJson(cols.readAll()).object();
        auto fromColor = [&] (const QString& key) {
            auto arr = obj[key].toArray();
            if(arr.size() == 3)
                return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt());
            else if(arr.size() == 4)
                return QColor(arr[0].toInt(), arr[1].toInt(), arr[2].toInt(), arr[3].toInt());
            ISCORE_ABORT;
            return QColor{};
        };

        instance.ConstraintBase = fromColor("ConstraintBase");
        instance.ConstraintSelected = fromColor("ConstraintSelected");
        instance.ConstraintPlayFill = fromColor("ConstraintPlayFill");
        instance.ConstraintWarning = fromColor("ConstraintWarning");
        instance.ConstraintInvalid = fromColor("ConstraintInvalid");
        instance.ConstraintDefaultLabel = fromColor("ConstraintDefaultLabel");
        instance.ConstraintDefaultBackground = fromColor("ConstraintDefaultBackground");

        instance.RackSideBorder = fromColor("RackSideBorder");

        instance.ConstraintFullViewParentSelected = fromColor("ConstraintFullViewParentSelected");

        instance.ConstraintHeaderText = fromColor("ConstraintHeaderText");
        instance.ConstraintHeaderBottomLine = fromColor("ConstraintHeaderBottomLine");
        instance.ConstraintHeaderRackHidden = fromColor("ConstraintHeaderRackHidden");
        instance.ConstraintHeaderSideBorder = fromColor("ConstraintHeaderSideBorder");

        instance.ProcessViewBorder = fromColor("ProcessViewBorder");

        instance.SlotFocus = fromColor("SlotFocus");
        instance.SlotOverlayBorder = fromColor("SlotOverlayBorder");
        instance.SlotOverlay = fromColor("SlotOverlay");
        instance.SlotHandle = fromColor("SlotHandle");

        instance.TimenodeDefault = fromColor("TimenodeDefault");
        instance.TimenodeSelected = fromColor("TimenodeSelected");

        instance.EventDefault = fromColor("EventDefault");
        instance.EventWaiting = fromColor("EventWaiting");
        instance.EventPending = fromColor("EventPending");
        instance.EventHappened = fromColor("EventHappened");
        instance.EventDisposed = fromColor("EventDisposed");
        instance.EventSelected = fromColor("EventSelected");

        instance.ConditionWaiting = fromColor("ConditionWaiting");
        instance.ConditionDisabled = fromColor("ConditionDisabled");
        instance.ConditionFalse = fromColor("ConditionFalse");
        instance.ConditionTrue = fromColor("ConditionTrue");

        instance.StateOutline = fromColor("StateOutline");
        instance.StateSelected = fromColor("StateSelected");

        instance.Background = fromColor("Background");
        instance.ProcessPanelBackground = fromColor("ProcessPanelBackground");

        instance.TimeRulerBackground = fromColor("TimeRulerBackground");
        instance.TimeRuler = fromColor("TimeRuler");
        instance.LocalTimeRuler = fromColor("LocalTimeRuler");
    }
}

const Scenario::ScenarioModel* ScenarioApplicationPlugin::focusedScenarioModel() const
{
    if(auto focusManager = processFocusManager())
    {
        return dynamic_cast<const Scenario::ScenarioModel*>(focusManager->focusedModel());
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
    for(const auto& action : pluginActions())
    {
        if(auto trsprt = dynamic_cast<TransportActions*>(action))
        {
            trsprt->stop();
            return;
        }
    }
}

ProcessFocusManager* ScenarioApplicationPlugin::processFocusManager() const
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

