#include "ScenarioControl.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"


#include "Control/OldFormatConversion.hpp"

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

ScenarioControl::ScenarioControl(QObject *parent) :
        PluginControlInterface{"ScenarioControl", parent},
        m_processList{new ProcessList{this}}
{

}

template<typename Selected_T>
auto arrayToJson(Selected_T &&selected)
{
    QJsonArray array;
    if (!selected.empty())
    {
        for (auto &element : selected)
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

    if (auto sm = focusedScenario())
    {
        base["Constraints"] = arrayToJson(selectedElements(sm->constraints()));
        base["Events"] = arrayToJson(selectedElements(sm->events()));
        base["TimeNodes"] = arrayToJson(selectedElements(sm->timeNodes()));
    }
    else
    {
        // Full-view copy
        auto& bem = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
        if(bem.constraintModel()->selection.get())
        {
            QJsonArray arr;
            Visitor<Reader<JSONObject>> jr;
            jr.readFrom(*bem.constraintModel());
            arr.push_back(jr.m_obj);
            base["Constraints"] = arr;
        }
    }
    return base;
}

QJsonObject ScenarioControl::cutSelectedElementsToJson()
{
    auto obj = copySelectedElementsToJson();

    if (auto sm = focusedScenario())
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
    if (auto sm = focusedScenario())
    {
        auto selectedConstraints = selectedElements(sm->constraints());
        for(const auto& json_vref : obj["Constraints"].toArray())
        {
            for(const auto& constraint : selectedConstraints)
            {
                auto cmd = new Scenario::Command::CopyConstraintContent{
                        json_vref.toObject(),
                        iscore::IDocument::path(constraint),
                        stateMachine().expandMode()};

                CommandDispatcher<> dispatcher{this->currentDocument()->commandStack()};
                dispatcher.submitCommand(cmd);
            }
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
                if (auto sm = focusedScenario())
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
                if (auto sm = focusedScenario())
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


// TODO use the one in ScenarioStateMachine
enum ScenarioAction
{
    Create, Move, DeckMove, Select
};

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

    return act;
}

ScenarioStateMachine& ScenarioControl::stateMachine() const
{
    auto &model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
    return static_cast<const TemporalScenarioViewModel *>(model.focusedViewModel())->stateMachine();
}

QList<OrderedToolbar> ScenarioControl::makeToolbars()
{
    // TODO make a method of this

    auto focusedScenarioViewModel = [this]() -> bool
    {
        if (!currentDocument())
        {
            return false;
        }

        auto &model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
        return dynamic_cast<const TemporalScenarioViewModel *>(model.focusedViewModel()) != nullptr;
    };

    QToolBar *bar = new QToolBar;
    // The tools
    m_scenarioToolActionGroup = new QActionGroup{bar};
    m_scenarioToolActionGroup->setEnabled(false);

    selecttool = makeToolbarAction(
                tr("Select"),
                m_scenarioToolActionGroup,
                ScenarioAction::Select,
                tr("Alt+x"));
    selecttool->setChecked(true);
    connect(selecttool, &QAction::triggered, [=]()
    { if (focusedScenarioViewModel()) stateMachine().setSelectState(); });

    auto createtool = makeToolbarAction(
                tr("Create"),
                m_scenarioToolActionGroup,
                ScenarioAction::Create,
                tr("Alt+c"));
    connect(createtool, &QAction::triggered, [=]()
    { if (focusedScenarioViewModel()) stateMachine().setCreateState(); });

    auto movetool = makeToolbarAction(
                tr("Move"),
                m_scenarioToolActionGroup,
                ScenarioAction::Move,
                tr("Alt+v"));
    connect(movetool, &QAction::triggered, [=]()
    { if (focusedScenarioViewModel()) stateMachine().setMoveState(); } );

    auto deckmovetool = makeToolbarAction(
                tr("Move Deck"),
                m_scenarioToolActionGroup,
                ScenarioAction::DeckMove,
                tr("Alt+b"));
    connect(deckmovetool, &QAction::triggered, [=]()
    { if (focusedScenarioViewModel()) stateMachine().setDeckMoveState(); });


    // The action modes
    m_scenarioScaleModeActionGroup = new QActionGroup{bar};

    auto scale = makeToolbarAction(
                tr("Scale"),
                m_scenarioScaleModeActionGroup,
                ExpandMode::Scale,
                tr("Alt+Shift+S"));
    scale->setChecked(true);
    connect(scale, &QAction::triggered, [=]()
    { if (focusedScenarioViewModel()) stateMachine().setScaleState(); });

    auto grow = makeToolbarAction(
                tr("Grow/Shrink"),
                m_scenarioScaleModeActionGroup,
                ExpandMode::Grow,
                tr("Alt+Shift+D"));
    connect(grow, &QAction::triggered, [=]()
    { if (focusedScenarioViewModel()) stateMachine().setGrowState(); });

    on_presenterChanged();

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

void ScenarioControl::on_presenterChanged()
{
    // Check the current focused view model of this document
    // If it is a scenario, we enable the actiongroup, else we disable it.
    if (!m_scenarioToolActionGroup)
    { return; }
    if (!currentDocument())
    {
        selecttool->setChecked(true);
        m_scenarioToolActionGroup->setEnabled(false);
        m_scenarioScaleModeActionGroup->setEnabled(false);
        return;
    }

    auto &model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());

    this->disconnect(m_toolbarConnection);
    m_toolbarConnection = connect(&model, &BaseElementModel::focusedViewModelChanged,
                                  this, [&]()
    {
        // Get the process viewmodel
        auto scenario = dynamic_cast<const TemporalScenarioViewModel *>(model.focusedViewModel());
        m_scenarioToolActionGroup->setEnabled(scenario);
        m_scenarioScaleModeActionGroup->setEnabled(scenario);
        if (scenario)
        {
            // Set the current state on the statemachine.
            // TODO put this in a pattern (MappedActionGroup?)
            for (QAction *action : m_scenarioToolActionGroup->actions())
            {
                if (action->isChecked())
                {
                    switch (action->data().toInt())
                    {
                        case ScenarioAction::Create:
                            scenario->stateMachine().setCreateState();
                            break;
                        case ScenarioAction::DeckMove:
                            scenario->stateMachine().setDeckMoveState();
                            break;
                        case ScenarioAction::Move:
                            scenario->stateMachine().setMoveState();
                            break;
                        case ScenarioAction::Select:
                            scenario->stateMachine().setSelectState();
                            break;

                        default:
                            Q_ASSERT(false);
                            break;
                    }
                }
            }

            for(QAction* action : m_scenarioScaleModeActionGroup->actions())
            {
                if (action->isChecked())
                {
                    switch (action->data().toInt())
                    {
                        case ExpandMode::Scale:
                            scenario->stateMachine().setScaleState();
                            break;
                        case ExpandMode::Grow:
                            scenario->stateMachine().setGrowState();
                            break;

                        default:
                            Q_ASSERT(false);
                            break;
                    }
                }
            }
        }

    });
}

void ScenarioControl::on_documentChanged()
{
    on_presenterChanged();
    if(!currentDocument())
    {
        m_scenarioScaleModeActionGroup->setDisabled(true);
        m_scenarioToolActionGroup->setDisabled(true);
        return;
    }

    auto &model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
    auto onScenario = dynamic_cast<const TemporalScenarioViewModel *>(model.focusedViewModel());

    selecttool->setChecked(true);
    m_scenarioToolActionGroup->setEnabled(onScenario);
    m_scenarioScaleModeActionGroup->setEnabled(onScenario);
}

const ScenarioModel *ScenarioControl::focusedScenario()
{
    auto& model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
    auto sm = dynamic_cast<const ProcessViewModelInterface*>(model.focusedViewModel());
    return sm ? dynamic_cast<const ScenarioModel*>(&sm->sharedProcessModel()) : nullptr;
}
