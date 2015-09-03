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

void ignore_template_instantiations()
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

void test_parse_expr_full();
ScenarioControl::ScenarioControl(iscore::Presenter* pres) :
    PluginControlInterface{pres, "ScenarioControl", nullptr},
    m_processList{this}
{
    setupCommands();

    m_objectAction = new ObjectMenuActions{iscore::ToplevelMenuElement::ObjectMenu, this};
    m_toolActions = new ToolMenuActions{iscore::ToplevelMenuElement::ToolMenu, this};
}


void ScenarioControl::populateMenus(iscore::MenubarManager *menu)
{
    ///// Edit /////
    m_objectAction->fillMenuBar(menu);

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

    ///// Tool /////

    m_toolActions->fillMenuBar(menu);
}

QList<OrderedToolbar> ScenarioControl::makeToolbars()
{
    QToolBar *bar = new QToolBar;

    m_toolActions->makeToolBar(bar);
    bar->addSeparator();

    return QList<OrderedToolbar>{OrderedToolbar(1, bar)};
}

iscore::SerializableCommand *ScenarioControl::instantiateUndoCommand(
        const QString& name,
        const QByteArray& data)
{
    return PluginControlInterface::instantiateUndoCommand<ScenarioCommandFactory>(name, data);
}


void ScenarioControl::createContextMenu(const QPoint& pos)
{
    QMenu contextMenu;

    contextMenu.addAction(m_selectAll);
    contextMenu.addAction(m_deselectAll);
    contextMenu.addSeparator();

    if(auto scenario = focusedScenarioModel())
    {
        auto selected = scenario->selectedChildren();

        for(AbstractMenuActions*& elt : m_pluginActions)
        {
            elt->fillContextMenu(&contextMenu, selected);
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
    m_toolActions->setEnabled(false);
    if(auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres))
    {
        s_pres->stateMachine().changeTool((int)ScenarioToolKind::Select);
    }
}


void ScenarioControl::on_presenterFocused(LayerPresenter* pres)
{
    // Get the scenario presenter
    auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres);
    m_toolActions->setEnabled(bool(s_pres));

    if(auto currentlyFocused = focusedPresenter())
    {
        disconnect(currentlyFocused, &TemporalScenarioPresenter::contextMenuAsked,
                   this, &ScenarioControl::createContextMenu);
    }
    if (s_pres)
    {
        connect(s_pres, &TemporalScenarioPresenter::keyPressed,
                m_toolActions,  &ToolMenuActions::keyPressed);

        connect(s_pres, &TemporalScenarioPresenter::keyReleased,
                m_toolActions,  &ToolMenuActions::keyReleased);

        connect(focusedPresenter(), &TemporalScenarioPresenter::contextMenuAsked,
                this, &ScenarioControl::createContextMenu);

        // Set the current state on the statemachine.
        // TODO put this in a pattern (MappedActionGroup?)
        for (QAction *action : m_toolActions->toolActions())
        {
            if (action->isChecked())
            {
                s_pres->stateMachine().changeTool(action->data().toInt());
            }
        }
    }
}


void ScenarioControl::on_documentChanged()
{
    this->disconnect(m_focusConnection);
    this->disconnect(m_defocusConnection);

    if(!currentDocument())
    {
        m_toolActions->setEnabled(false);
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

