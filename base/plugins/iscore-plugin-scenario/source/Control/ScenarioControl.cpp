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
#include "ScenarioCommandFactory.hpp"

#include "Process/Temporal/TemporalScenarioPresenter.hpp"

#include <iscore/command/CommandGeneratorMap.hpp>

#include "Menus/ObjectMenuActions.hpp"
#include "Menus/ToolMenuActions.hpp"

#include <core/document/DocumentModel.hpp>

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

//    m_objectAction = new ObjectMenuActions{iscore::ToplevelMenuElement::ObjectMenu, this};
//    m_toolActions = new ToolMenuActions{iscore::ToplevelMenuElement::ToolMenu, this};

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
}


ScenarioControl* ScenarioControl::instance(Presenter* pres)
{
    static auto ctrl = new ScenarioControl(pres);
    return ctrl;
}

void ScenarioControl::populateMenus(iscore::MenubarManager *menu)
{
    ///// Edit /////
//    m_objectAction->fillMenuBar(menu);

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

        act->makeToolBar(bar);

        if(i < m_pluginActions.size() - 1)
            bar->addSeparator();

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

void ScenarioControl::createContextMenu(const QPoint& pos, const QPointF& scenepos)
{
    QMenu contextMenu;

    contextMenu.addAction(m_selectAll);
    contextMenu.addAction(m_deselectAll);
    contextMenu.addSeparator();

    if(auto scenario = focusedScenarioModel())
    {
        auto selected = scenario->selectedChildren();

        for(ScenarioActions*& elt : m_pluginActions)
        {
            // TODO make a class to encapsulate all the data
            // required to set-up a context menu in a scenario.
            elt->fillContextMenu(&contextMenu, selected, focusedPresenter(), pos, scenepos);
            contextMenu.addSeparator();
        }
    }
    contextMenu.exec(pos);

    contextMenu.close();
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
}


void ScenarioControl::on_presenterFocused(LayerPresenter* pres)
{
    // Get the scenario presenter
    auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres);

    for(ScenarioActions*& elt : m_pluginActions)
    {
        elt->setEnabled(bool(s_pres));
    }

    if(auto currentlyFocused = focusedPresenter())
    {
        disconnect(currentlyFocused, &TemporalScenarioPresenter::contextMenuAsked,
                   this, &ScenarioControl::createContextMenu);
    }
    if (s_pres)
    {
        connect(s_pres, &TemporalScenarioPresenter::keyPressed,
                this,  &ScenarioControl::keyPressed);

        connect(s_pres, &TemporalScenarioPresenter::keyReleased,
                this,  &ScenarioControl::keyReleased);

        connect(focusedPresenter(), &TemporalScenarioPresenter::contextMenuAsked,
                this, &ScenarioControl::createContextMenu);

        // Set the current state on the statemachine.
        // TODO put this in a pattern (MappedActionGroup?)
  /*      for (QAction *action : m_toolActions->toolActions())
        {
            if (action->isChecked())
            {
                s_pres->stateMachine().changeTool(action->data().toInt());
            }
        }*/
    }
}


void ScenarioControl::on_documentChanged()
{
    this->disconnect(m_focusConnection);
    this->disconnect(m_defocusConnection);

    if(!currentDocument())
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

        on_presenterFocused(focusManager->focusedPresenter());
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

