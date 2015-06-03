#include "ScenarioControl.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/StateMachines/Tool.hpp"

#include "Control/OldFormatConversion.hpp"

#include "Menus/ObjectMenuActions.hpp"
#include "Menus/ToolMenuActions.hpp"

#include <core/document/DocumentModel.hpp>

#include <QToolBar>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QApplication>
#include <QClipboard>


using namespace iscore;

ScenarioControl::ScenarioControl(iscore::Presenter* pres) :
    PluginControlInterface{pres, "ScenarioControl", nullptr},
    m_processList{this}
{
    m_objectAction = new ObjectMenuActions{iscore::ToplevelMenuElement::ObjectMenu, this};
    m_toolActions = new ToolMenuActions{iscore::ToplevelMenuElement::ToolMenu, this};
}

template<typename Selected_T>
auto arrayToJson(Selected_T &&selected)
{
    QJsonArray array;
    if (!selected.empty())
    {
        for (const auto &element : selected)
        {
            Visitor<Reader<JSONObject>> jr;
            jr.readFrom(*element);
            array.push_back(jr.m_obj);
        }
    }

    return array;
}

QJsonObject ScenarioControl::copySelectedElementsToJson()
{
    QJsonObject base;

    if (auto sm = focusedScenarioModel())
    {
        base["Constraints"] = arrayToJson(selectedElements(sm->constraints()));
        base["Events"] = arrayToJson(selectedElements(sm->events()));
        base["TimeNodes"] = arrayToJson(selectedElements(sm->timeNodes()));
    }
    else
    {
        // Full-view copy
        auto& bem = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
        if(bem.baseConstraint()->selection.get())
        {
            QJsonArray arr;
            Visitor<Reader<JSONObject>> jr;
            jr.readFrom(*bem.baseConstraint());
            arr.push_back(jr.m_obj);
            base["Constraints"] = arr;
        }
    }
    return base;
}

QJsonObject ScenarioControl::cutSelectedElementsToJson()
{
    auto obj = copySelectedElementsToJson();

    if (auto sm = focusedScenarioModel())
    {
        ScenarioGlobalCommandManager mgr{currentDocument()->commandStack()};
        mgr.clearContentFromSelection(*sm);
    }

    return obj;
}

#include <Commands/Constraint/CopyConstraintContent.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
void ScenarioControl::writeJsonToSelectedElements(const QJsonObject& obj)
{
    auto pres = focusedPresenter();
    if(!pres)
        return;

    auto sm = focusedScenarioModel();

    auto selectedConstraints = selectedElements(sm->constraints());
    for(const auto& json_vref : obj["Constraints"].toArray())
    {
        for(const auto& constraint : selectedConstraints)
        {
            auto cmd = new Scenario::Command::CopyConstraintContent{
                       json_vref.toObject(),
                       iscore::IDocument::path(constraint),
                       pres->stateMachine().expandMode()};

            CommandDispatcher<> dispatcher{this->currentDocument()->commandStack()};
            dispatcher.submitCommand(cmd);
        }
    }

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

template<typename Data>
QAction* makeToolbarAction(const QString& name,
                           QObject* parent,
                           const Data& data,
                           const QString& shortcut)
{
    auto act = new QAction{name, parent};
    act->setCheckable(true);
    act->setData(QVariant::fromValue((int) data));
    act->setShortcutContext(Qt::ApplicationShortcut);
    act->setShortcut(shortcut);
    act->setToolTip(name+ " (" + shortcut + ")");

    return act;
}

ScenarioStateMachine& ScenarioControl::stateMachine() const
{
    return focusedPresenter()->stateMachine();
}

QList<OrderedToolbar> ScenarioControl::makeToolbars()
{
    QToolBar *bar = new QToolBar;

    // TODO : put it in ToolMenuActions
    m_shiftActionGroup = new QActionGroup{bar};
    m_shiftActionGroup->setDisabled(true);

    auto verticalmove = makeToolbarAction(
                            tr("Vertical Move"),
                            m_shiftActionGroup,
                            ExpandMode::Fixed,
                            tr("Shift"));
    connect(verticalmove, &QAction::toggled, [=] ()
    {
        if(focusedPresenter())
        {
            if (verticalmove->isChecked())
                focusedPresenter()->stateMachine().shiftPressed();
            else
                focusedPresenter()->stateMachine().shiftReleased();
        }
    });

    m_shiftActionGroup->setDisabled(true);

    m_toolActions->makeToolBar(bar);
    bar->addSeparator();
    bar->addActions(m_shiftActionGroup->actions());

    return QList<OrderedToolbar>{OrderedToolbar(1, bar)};
}


// Defined in CommandNames.cpp
iscore::SerializableCommand *makeCommandByName(const QString &name);

iscore::SerializableCommand *ScenarioControl::instantiateUndoCommand(const QString &name, const QByteArray &data)
{
    iscore::SerializableCommand *cmd = makeCommandByName(name);
    if (!cmd)
    {
        qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
        return nullptr;
    }

    cmd->deserialize(data);
    return cmd;
}

void ScenarioControl::createContextMenu(const QPoint& pos)
{
    QMenu contextMenu {};
    contextMenu.clear();

    contextMenu.addAction(m_selectAll);
    contextMenu.addAction(m_deselectAll);
    contextMenu.addSeparator();

    if(focusedScenarioModel())
    {
        auto selectedCstr = selectedElements(focusedScenarioModel()->constraints());
        auto selectedEvt = selectedElements(focusedScenarioModel()->events());
        auto selectedTn = selectedElements(focusedScenarioModel()->timeNodes());

        if(!selectedCstr.empty() || !selectedEvt.empty() || !selectedTn.empty() )
        {
            m_objectAction->fillContextMenu(&contextMenu);
            contextMenu.addSeparator();
        }
    }
    m_toolActions->fillContextMenu(&contextMenu);

    contextMenu.exec(pos);

    contextMenu.close();
}

void ScenarioControl::on_presenterDefocused(ProcessPresenter* pres)
{
    // We set the currently focused view model to a "select" state
    // to prevent problems.
    m_toolActions->setEnabled(false);
    m_shiftActionGroup->setEnabled(false);
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

    m_shiftActionGroup->setEnabled(s_pres);

    disconnect(focusedPresenter(), &TemporalScenarioPresenter::contextMenuAsked,
               this, &ScenarioControl::createContextMenu);

    if (s_pres)
    {
        connect(s_pres, &TemporalScenarioPresenter::shiftPressed,
                this, [&]()
        {
            for(QAction* action : m_shiftActionGroup->actions())
            {
                if(action->data().toInt() == ExpandMode::Fixed)
                {
                    action->setChecked(true);
                }
            }
        });

        connect(s_pres, &TemporalScenarioPresenter::shiftReleased,
                this, [&]()
        {
            for(QAction* action : m_shiftActionGroup->actions())
            {
                if(action->data().toInt() == ExpandMode::Fixed)
                {
                    action->setChecked(false);
                }
            }
        });

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

