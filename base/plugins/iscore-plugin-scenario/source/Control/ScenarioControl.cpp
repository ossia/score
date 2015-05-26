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

#include <core/document/DocumentModel.hpp>

#include <QToolBar>
#include <QFile>
#include <QFileDialog>
#include <QTextBlock>
#include <QJsonDocument>
#include <QGridLayout>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QApplication>
#include <QClipboard>
class TextDialog : public QDialog
{
    public:
        TextDialog(QString s)
        {
            this->setLayout(new QGridLayout);
            auto textEdit = new QTextEdit;
            textEdit->setPlainText(s);
            layout()->addWidget(textEdit);
            auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
            layout()->addWidget(buttonBox);

            connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

        }
};

using namespace iscore;

ScenarioControl::ScenarioControl(iscore::Presenter* pres) :
    PluginControlInterface{pres, "ScenarioControl", nullptr},
    m_processList{this}
{
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
    // Remove
    QAction *removeElements = new QAction{tr("Remove scenario elements"), this};
    removeElements->setShortcut(QKeySequence::Delete);
    connect(removeElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{currentDocument()->commandStack()};
            mgr.deleteSelection(*sm);
        }
    });
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       removeElements);


    QAction *clearElements = new QAction{tr("Clear scenario elements"), this};
    clearElements->setShortcut(Qt::Key_Backspace);
    connect(clearElements, &QAction::triggered,
            [this]()
    {
        if (auto sm = focusedScenarioModel())
        {
            ScenarioGlobalCommandManager mgr{currentDocument()->commandStack()};
            mgr.clearContentFromSelection(*sm);
        }
    });
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       clearElements);

    // Copy-paste
    menu->addSeparatorIntoToplevelMenu(ToplevelMenuElement::EditMenu, EditMenuElement::Separator_Copy);
    QAction *copyConstraintContent = new QAction{tr("Copy"), this};
    copyConstraintContent->setShortcut(QKeySequence::Copy);
    connect(copyConstraintContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{copySelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       copyConstraintContent);

    QAction *cutConstraintContent = new QAction{tr("Cut"), this};
    cutConstraintContent->setShortcut(QKeySequence::Cut);
    connect(cutConstraintContent, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{cutSelectedElementsToJson()};
        auto clippy = QApplication::clipboard();
        clippy->setText(doc.toJson(QJsonDocument::Indented));
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       cutConstraintContent);

    QAction *pasteConstraintContent = new QAction{tr("Paste"), this};
    pasteConstraintContent->setShortcut(QKeySequence::Paste);
    connect(pasteConstraintContent, &QAction::triggered,
            [this]()
    {
        writeJsonToSelectedElements(
                    QJsonDocument::fromJson(
                        QApplication::clipboard()->text().toLatin1()).object());
    });
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       pasteConstraintContent);

    ///// View /////
    QAction *selectAll = new QAction{tr("Select all"), this};
    selectAll->setShortcut(QKeySequence::SelectAll);
    selectAll->setToolTip("Ctrl+a");
    connect(selectAll, &QAction::triggered,
            [this]()
    {
        auto &pres = IDocument::presenterDelegate<BaseElementPresenter>(*currentDocument());
        pres.selectAll();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       selectAll);


    QAction *deselectAll = new QAction{tr("Deselect all"), this};
    deselectAll->setShortcut(QKeySequence::Deselect);
    deselectAll->setToolTip("Ctrl+Shift+a");
    connect(deselectAll, &QAction::triggered,
            [this]()
    {
        auto &pres = IDocument::presenterDelegate<BaseElementPresenter>(*currentDocument());
        pres.deselectAll();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       deselectAll);

    QAction *elementsToJson = new QAction{tr("Convert selection to JSON"), this};
    connect(elementsToJson, &QAction::triggered,
            [this]()
    {
        QJsonDocument doc{copySelectedElementsToJson()};
        auto s = new TextDialog(doc.toJson(QJsonDocument::Indented));

        s->show();
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       elementsToJson);
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
    act->setToolTip(shortcut);

    return act;
}

ScenarioStateMachine& ScenarioControl::stateMachine() const
{
    return focusedPresenter()->stateMachine();
}

QList<OrderedToolbar> ScenarioControl::makeToolbars()
{
    QToolBar *bar = new QToolBar;
    // The tools
    m_scenarioToolActionGroup = new QActionGroup{bar};
    m_scenarioToolActionGroup->setDisabled(true);

    // NOTE : if a scenario isn't focused, they shouldn't event be clickable.
    m_selecttool = makeToolbarAction(
                     tr("Select"),
                     m_scenarioToolActionGroup,
                     Tool::Select,
                     tr("Alt+x"));
    m_selecttool->setChecked(true);
    connect(m_selecttool, &QAction::triggered, [=]()
    { focusedPresenter()->stateMachine().changeTool(static_cast<int>(Tool::Select)); });

    auto createtool = makeToolbarAction(
                          tr("Create"),
                          m_scenarioToolActionGroup,
                          Tool::Create,
                          tr("Alt+c"));
    connect(createtool, &QAction::triggered, [=]()
    { focusedPresenter()->stateMachine().changeTool(static_cast<int>(Tool::Create)); });

    auto movetool = makeToolbarAction(
                        tr("Move"),
                        m_scenarioToolActionGroup,
                        Tool::Move,
                        tr("Alt+v"));
    connect(movetool, &QAction::triggered, [=]()
    { focusedPresenter()->stateMachine().changeTool(static_cast<int>(Tool::Move)); } );

    auto deckmovetool = makeToolbarAction(
                            tr("Move Deck"),
                            m_scenarioToolActionGroup,
                            Tool::MoveDeck,
                            tr("Alt+b"));
    connect(deckmovetool, &QAction::triggered, [=]()
    { focusedPresenter()->stateMachine().changeTool(static_cast<int>(Tool::MoveDeck)); });

    // The action modes
    m_scenarioScaleModeActionGroup = new QActionGroup{bar};
    m_scenarioScaleModeActionGroup->setDisabled(true);

    auto scale = makeToolbarAction(
                     tr("Scale"),
                     m_scenarioScaleModeActionGroup,
                     ExpandMode::Scale,
                     tr("Alt+S"));
    scale->setChecked(true);
    connect(scale, &QAction::triggered, [=]()
    { focusedPresenter()->stateMachine().setScaleState(); });

    auto grow = makeToolbarAction(
                    tr("Grow/Shrink"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Grow,
                    tr("Alt+D"));
    connect(grow, &QAction::triggered, [=]()
    { focusedPresenter()->stateMachine().setGrowState(); });

    auto verticalmove = makeToolbarAction(
                            tr("Vertical Move"),
                            m_scenarioScaleModeActionGroup,
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

    m_scenarioToolActionGroup->setDisabled(true);
    m_scenarioScaleModeActionGroup->setDisabled(true);

    bar->addActions(m_scenarioToolActionGroup->actions());
    bar->addSeparator();
    bar->addActions(m_scenarioScaleModeActionGroup->actions());

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

void ScenarioControl::on_presenterDefocused(ProcessPresenter* pres)
{
    // We set the currently focused view model to a "select" state
    // to prevent problems.
    m_scenarioToolActionGroup->setEnabled(false);
    m_scenarioScaleModeActionGroup->setEnabled(false);
    if(auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres))
    {
        s_pres->stateMachine().changeTool((int)Tool::Select);
    }
}


void ScenarioControl::on_presenterFocused(ProcessPresenter* pres)
{
    // Get the scenario presenter
    auto s_pres = dynamic_cast<TemporalScenarioPresenter*>(pres);
    m_selecttool->setChecked(true);
    m_scenarioToolActionGroup->setEnabled(s_pres);
    m_scenarioScaleModeActionGroup->setEnabled(s_pres);
    if (s_pres)
    {
        connect(s_pres, &TemporalScenarioPresenter::shiftPressed,
                this, [&]()
        {
            for(QAction* action : m_scenarioScaleModeActionGroup->actions())
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
            for(QAction* action : m_scenarioScaleModeActionGroup->actions())
            {
                if(action->data().toInt() == ExpandMode::Fixed)
                {
                    action->setChecked(false);
                }
                if(action->data().toInt() == ExpandMode::Scale)
                {
                    action->setChecked(true);
                }
            }
        });

        // Set the current state on the statemachine.
        // TODO put this in a pattern (MappedActionGroup?)
        for (QAction *action : m_scenarioToolActionGroup->actions())
        {
            if (action->isChecked())
            {
                s_pres->stateMachine().changeTool(action->data().toInt());
            }
        }

        for(QAction* action : m_scenarioScaleModeActionGroup->actions())
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
                        s_pres->stateMachine().shiftPressed();
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
        m_scenarioScaleModeActionGroup->setDisabled(true);
        m_scenarioToolActionGroup->setDisabled(true);
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

