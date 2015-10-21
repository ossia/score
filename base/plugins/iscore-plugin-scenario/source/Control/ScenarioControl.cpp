#include "ScenarioControl.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackModel.hpp"
#include "Document/Constraint/Rack/Slot/SlotModel.hpp"
#include <ProcessInterface/Process.hpp>
#include <ProcessInterface/LayerModel.hpp>
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/State/StateModel.hpp"
#include "Process/ScenarioModel.hpp"

#include "Process/Temporal/TemporalScenarioPresenter.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>

#include "Menus/ObjectMenuActions.hpp"
#include "Menus/ToolMenuActions.hpp"

#include <core/document/DocumentModel.hpp>
#include <ProcessInterface/Style/ScenarioStyle.hpp>
#include <QToolBar>
#include <QFile>
#include <QFileDialog>
#include <QApplication>

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
#include "Control/Menus/ScenarioCommonContextMenuFactory.hpp"

void test_parse_expr_full();
ScenarioControl::ScenarioControl(iscore::Presenter* pres) :
    PluginControlInterface{pres, "ScenarioControl", nullptr},
    m_processList{this},
    m_moveEventList{this}
{
    setupCommands();

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


ScenarioControl* ScenarioControl::instance(Presenter* pres)
{
    static auto ctrl = new ScenarioControl(pres);
    return ctrl;
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

#include "Menus/TransportActions.hpp"
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

#include <Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <ViewCommands/PutLayerModelToFront.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationPresenter.hpp>
void ScenarioControl::createContextMenu(
        const QPoint& pos,
        const QPointF& scenepos,
        const LayerPresenter& pres)
{
    QMenu menu;

    // Fill with slot actions

    if(auto slotp = dynamic_cast<SlotPresenter*>(pres.parent()))
    {
        auto& slotm = slotp->model();
        auto processes_submenu = menu.addMenu(tr("Focus process"));
        for(const LayerModel& proc : slotm.layers)
        {
            auto name = proc.processModel().metadata.name();
            // TODO instead have a displayName() virtual method on models.
            if(auto autom = dynamic_cast<const AutomationPresenter*>(&pres))
            {
                auto autom_model = dynamic_cast<AutomationModel*>(&autom->layerModel().processModel());
                name += " : " + autom_model->address().toString();
            }
            QAction* procAct = new QAction{name, processes_submenu};
            connect(procAct, &QAction::triggered, this, [&] () {
                PutLayerModelToFront cmd{slotm, proc.id()};
                cmd.redo();
            } );
            processes_submenu->addAction(procAct);
        }
    }

    // Then the process-specific part
    pres.fillContextMenu(&menu, pos, scenepos);

    menu.exec(pos);
    menu.close();
}

void ScenarioControl::createScenarioContextMenu(
        QMenu& menu,
        const QPoint& pos,
        const QPointF& scenepos,
        const TemporalScenarioPresenter& pres)
{
    auto selected = pres.layerModel().processModel().selectedChildren();

    for(ScenarioActions*& elt : m_pluginActions)
    {
        // TODO make a class to encapsulate all the data
        // required to set-up a context menu in a scenario.
        elt->fillContextMenu(&menu, selected, pres, pos, scenepos);
        menu.addSeparator();
    }

    menu.addSeparator();
    menu.addAction(m_selectAll);
    menu.addAction(m_deselectAll);

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

    if(auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres))
    {
        s_pres->stateMachine().changeTool((int)ScenarioToolKind::Select);
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
                this, [=] (const QPoint& pt1, const QPointF& pt2) {
            createContextMenu(pt1, pt2, *pres);
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

#include <Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.hpp>
#include <Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <Document/Constraint/Rack/RackPresenter.hpp>
#include <Document/Constraint/Rack/Slot/SlotPresenter.hpp>
#include <core/document/DocumentView.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateViewInterface.hpp>
#include <Document/BaseElement/BaseElementView.hpp>
void ScenarioControl::on_documentChanged()
{
    this->disconnect(m_focusConnection);
    this->disconnect(m_defocusConnection);

    auto doc = currentDocument();
    if(!doc)
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
            auto& pres = IDocument::presenterDelegate<BaseElementPresenter>(*doc);
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
                        const auto& front_proc = first.model().frontLayerModel();
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
        auto bev = dynamic_cast<BaseElementView*>(doc->view().viewDelegate());
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

        qDebug() << instance.Background;
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

