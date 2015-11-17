#include "ScenarioControl.hpp"
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Process/Process.hpp>
#include <Process/LayerModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <QJsonDocument>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>

#include <iscore/command/CommandGeneratorMap.hpp>

#include "Menus/ObjectMenuActions.hpp"
#include "Menus/ToolMenuActions.hpp"

#include <core/document/DocumentModel.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <QToolBar>
#include <QFile>
#include <QFileDialog>
#include <QApplication>

#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <core/application/Application.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>
#include "Menus/TransportActions.hpp"

// This part is somewhat similar to what moc does
// with moc_.. stuff generation.
#include <iscore/tools/NotifyingMap_impl.hpp>

template void NotifyingMap<LayerModel>::add(LayerModel*);
template void NotifyingMap<SlotModel>::add(SlotModel*);
template void NotifyingMap<RackModel>::add(RackModel*);
template void NotifyingMap<Process>::add(Process*);
template void NotifyingMap<ConstraintModel>::add(ConstraintModel*);
template void NotifyingMap<EventModel>::add(EventModel*);
template void NotifyingMap<TimeNodeModel>::add(TimeNodeModel*);
template void NotifyingMap<StateModel>::add(StateModel*);

template void NotifyingMap<LayerModel>::remove(LayerModel*);
template void NotifyingMap<SlotModel>::remove(SlotModel*);
template void NotifyingMap<RackModel>::remove(RackModel*);
template void NotifyingMap<Process>::remove(Process*);
template void NotifyingMap<ConstraintModel>::remove(ConstraintModel*);
template void NotifyingMap<EventModel>::remove(EventModel*);
template void NotifyingMap<TimeNodeModel>::remove(TimeNodeModel*);
template void NotifyingMap<StateModel>::remove(StateModel*);

template void NotifyingMap<LayerModel>::remove(const Id<LayerModel>&);
template void NotifyingMap<SlotModel>::remove(const Id<SlotModel>&);
template void NotifyingMap<RackModel>::remove(const Id<RackModel>&);
template void NotifyingMap<Process>::remove(const Id<Process>&);
template void NotifyingMap<ConstraintModel>::remove(const Id<ConstraintModel>&);
template void NotifyingMap<EventModel>::remove(const Id<EventModel>&);
template void NotifyingMap<TimeNodeModel>::remove(const Id<TimeNodeModel>&);
template void NotifyingMap<StateModel>::remove(const Id<StateModel>&);

void ignore_template_instantiations_Scenario()
{
    NotifyingMapInstantiations_T<LayerModel>();
    NotifyingMapInstantiations_T<SlotModel>();
    NotifyingMapInstantiations_T<RackModel>();
    NotifyingMapInstantiations_T<Process>();
    NotifyingMapInstantiations_T<ConstraintModel>();
    NotifyingMapInstantiations_T<EventModel>();
    NotifyingMapInstantiations_T<TimeNodeModel>();
    NotifyingMapInstantiations_T<StateModel>();
}

using namespace iscore;
#include <State/Expression.hpp>
#include <Scenario/Control/Menus/ScenarioCommonContextMenuFactory.hpp>

void test_parse_expr_full();
ScenarioControl::ScenarioControl(iscore::Application& app) :
    PluginControlInterface{app, "ScenarioControl", nullptr}
{
    connect(this, &ScenarioControl::defocused,
            this, &ScenarioControl::reinit_tools);

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

void ScenarioControl::populateMenus(iscore::MenubarManager *menu)
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
        auto &pres = IDocument::presenterDelegate<BaseElementPresenter>(*currentDocument());
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
        auto &pres = IDocument::presenterDelegate<BaseElementPresenter>(*currentDocument());
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

QList<OrderedToolbar> ScenarioControl::makeToolbars()
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


    return QList<OrderedToolbar>{OrderedToolbar(1, bar)};
}

QList<QAction*> ScenarioControl::actions()
{
    // TODO add the others
    QList<QAction*> act;
    for(const auto& elt : m_pluginActions)
    {
        act += elt->actions();
    }
    return act;
}

void ScenarioControl::on_presenterDefocused(LayerPresenter* pres)
{
    // We set the currently focused view model to a "select" state
    // to prevent problems.

    reinit_tools();

    for(ScenarioActions*& elt : m_pluginActions)
    {
        elt->setEnabled(false);
    }

    if(dynamic_cast<TemporalScenarioPresenter*>(pres))
    {
        // TODO this may not be necessary anymore since this is duplicated in on_focused.
        editionSettings().setTool(Scenario::Tool::Select);
    }

    disconnect(m_contextMenuConnection);
}


void ScenarioControl::on_presenterFocused(LayerPresenter* pres)
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

    for(ScenarioActions*& elt : m_pluginActions)
    {
        elt->setEnabled(bool(s_pres));
    }

    if (s_pres)
    {
        connect(s_pres, &TemporalScenarioPresenter::keyPressed,
                this,  &ScenarioControl::keyPressed);

        connect(s_pres, &TemporalScenarioPresenter::keyReleased,
                this,  &ScenarioControl::keyReleased);


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

void ScenarioControl::on_documentChanged(
        iscore::Document* olddoc,
        iscore::Document* newdoc)
{
    // TODO the context menu connection should be reviewed, too.
    this->disconnect(m_focusConnection);
    this->disconnect(m_defocusConnection);

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
                        this, &ScenarioControl::on_presenterFocused);
        m_defocusConnection =
                connect(focusManager, &ProcessFocusManager::sig_defocusedPresenter,
                        this, &ScenarioControl::on_presenterDefocused);

        if(focusManager->focusedPresenter())
        {
            // Used when switching between documents
            on_presenterFocused(focusManager->focusedPresenter());
        }
        else
        {
            // We focus by default the first process of the constraint in full view we're in
            // TODO this snippet is useful, put it somewhere in some Toolkit file.
            auto& pres = IDocument::presenterDelegate<BaseElementPresenter>(*newdoc);
            auto& cst = pres.displayedConstraint();
            if(!cst.processes.empty())
            {
                const auto& slts = pres.presenters().constraintPresenter()->rack()->getSlots();
                if(!slts.empty())
                {
                    const auto& top_slot = pres.presenters().constraintPresenter()->rack()->model().slotsPositions().front();
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
                                focusManager->setFocusedPresenter(proc_elt.processes.front().first);
                            }
                        }
                    }
                }
            }
        }

        // Finally we focus the View widget.
        auto bev = dynamic_cast<BaseElementView*>(newdoc->view().viewDelegate());
        if(bev)
            bev->view()->setFocus();
    }
}

void ScenarioControl::initColors()
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

const ScenarioModel* ScenarioControl::focusedScenarioModel() const
{
    return dynamic_cast<const ScenarioModel*>(processFocusManager()->focusedModel());
}

TemporalScenarioPresenter* ScenarioControl::focusedPresenter() const
{
    return dynamic_cast<TemporalScenarioPresenter*>(processFocusManager()->focusedPresenter());
}


void ScenarioControl::reinit_tools()
{
    emit keyReleased(Qt::Key_Control);
    emit keyReleased(Qt::Key_Shift);
}

void ScenarioControl::prepareNewDocument()
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

ProcessFocusManager* ScenarioControl::processFocusManager() const
{
    if(auto doc = currentDocument())
    {
        auto bem = dynamic_cast<BaseElementModel*>(doc->model().modelDelegate());
        if(bem)
        {
            return &bem->focusManager();
        }
    }

    return nullptr;
}

