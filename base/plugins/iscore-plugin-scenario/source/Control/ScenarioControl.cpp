#include "ScenarioControl.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/ScenarioModel.hpp"
//#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
//#include "Process/Temporal/TemporalScenarioView.hpp"
//#include "Process/Temporal/StateMachines/Tool.hpp"
#include <iscore/command/CommandGeneratorMap.hpp>

#include "Control/OldFormatConversion.hpp"

#include "Menus/ObjectMenuActions.hpp"
#include "Menus/ToolMenuActions.hpp"

#include <core/document/DocumentModel.hpp>

#include <QToolBar>
#include <QFile>
#include <QFileDialog>
#include <QApplication>
//#include <QClipboard>


using namespace iscore;

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
    ///// File /////
    // Export in old format
    auto toZeroTwo = new QAction("To i-score 0.2", this);
    connect(toZeroTwo, &QAction::triggered,
            [this]()
    {
        auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save"));

        if (!savename.isEmpty())
        {
            QFile f(savename);
            f.open(QIODevice::WriteOnly);
            f.write(JSONToZeroTwo(currentDocument()->saveAsJson()).toLatin1().constData());
        }
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Quit,
                                       toZeroTwo);


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

#include "ScenarioCommandFactory.hpp"

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

    if(focusedScenarioModel()) // TODO weird
    {
        auto selectedCstr = selectedElements(focusedScenarioModel()->constraints());
        auto selectedEvt = selectedElements(focusedScenarioModel()->events());
        auto selectedTn = selectedElements(focusedScenarioModel()->timeNodes());
        auto selectedSt = selectedElements(focusedScenarioModel()->states());

        m_objectAction->setConstraintAction(! selectedCstr.empty());
        if(!selectedCstr.empty() || !selectedEvt.empty() || !selectedTn.empty() || !selectedSt.empty() )
        {
            m_objectAction->fillContextMenu(&contextMenu);
            contextMenu.addSeparator();
        }
    }
    else
    {
        // should never happen
        Q_ASSERT(false);
    }

    m_toolActions->fillContextMenu(&contextMenu);

    // TODO allow plug-ins to insert data from here.

    contextMenu.exec(pos);

    contextMenu.close();
}

void ScenarioControl::on_presenterDefocused(ProcessPresenter* pres)
{
    // We set the currently focused view model to a "select" state
    // to prevent problems.
    m_toolActions->setEnabled(false);
    if(auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres))
    {
        s_pres->stateMachine().changeTool((int)Tool::Select);
    }
}


void ScenarioControl::on_presenterFocused(ProcessPresenter* pres)
{
    // Get the scenario presenter
    auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres);
    m_toolActions->setEnabled(s_pres);

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

        for(QAction* action : m_toolActions->modeActions())
        {
            if (action->isChecked())
            {
                switch (action->data().toInt())
                {
                    case ExpandMode::Scale:
                        s_pres->stateMachine().setScaleState();
                        break;
                    case ExpandMode::Grow:
                        s_pres->stateMachine().setGrowState();
                        break;
                    case ExpandMode::Fixed:
                        s_pres->stateMachine().setFixedState();
                        break;


                    default:
                        Q_ASSERT(false);
                        break;
                }
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
        auto bem = dynamic_cast<BaseElementModel*>(doc->model()->modelDelegate());
        if(bem)
        {
            return &bem->focusManager();
        }
    }

    return nullptr;
}

