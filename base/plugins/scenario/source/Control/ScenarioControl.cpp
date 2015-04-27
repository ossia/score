#include "ScenarioControl.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"
#include "Process/ScenarioModel.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"


#include "Control/OldFormatConversion.hpp"

#include <QFile>
#include <QFileDialog>
#include <QTextBlock>
#include <QJsonDocument>
#include <QGridLayout>
#include <QTextEdit>
#include <QDialogButtonBox>

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

void ScenarioControl::populateMenus(iscore::MenubarManager *menu)
{
    auto focusedScenario = [this]()
    {
        auto &model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
        return dynamic_cast<ScenarioModel *>(model.focusedViewModel()->sharedProcessModel());
    };

    // File

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


    // Edit
    QAction *removeElements = new QAction{tr("Remove scenario elements"), this};
    connect(removeElements, &QAction::triggered,
            [this, focusedScenario]()
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
    connect(clearElements, &QAction::triggered,
            [this, focusedScenario]()
            {
                if (auto sm = focusedScenario())
                {
                    ScenarioGlobalCommandManager mgr{currentDocument()->commandStack()};
                    mgr.clearContentFromSelection(*sm);
                }
            });
    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       clearElements);

    // View
    QAction *selectAll = new QAction{tr("Select all"), this};
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
            [this, focusedScenario]()
            {
                if (auto sm = focusedScenario())
                {
                    auto arrayToJson = [](auto &&selected)
                    {
                        QJsonArray array;
                        if (!selected.empty())
                        {
                            for (auto &element : selected)
                            {
                                Visitor<Reader<JSON>> jr;
                                jr.readFrom(*element);
                                array.push_back(jr.m_obj);
                            }
                        }

                        return array;
                    };


                    QJsonObject base;
                    base["Constraints"] = arrayToJson(selectedElements(sm->constraints()));
                    base["Events"] = arrayToJson(selectedElements(sm->events()));
                    base["TimeNodes"] = arrayToJson(selectedElements(sm->timeNodes()));


                    QJsonDocument doc;
                    doc.setObject(base);
                    auto s = new TextDialog(doc.toJson(QJsonDocument::Indented));

                    s->show();
                }
            });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                       ViewMenuElement::Windows,
                                       elementsToJson);
}

#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include <QToolBar>

// TODO use the one in ScenarioStateMachine
enum ScenarioAction
{
    Create, Move, DeckMove, Select
};


QList<QToolBar *> ScenarioControl::makeToolbars()
{
    // TODO make a method of this

    auto focusedScenario = [this]() -> bool
    {
        if (!currentDocument())
        {
            return false;
        }

        auto &model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
        return dynamic_cast<TemporalScenarioViewModel *>(model.focusedViewModel()) != nullptr;
    };

    auto focusedScenarioStateMachine = [this]() -> ScenarioStateMachine &
    {
        auto &model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
        return static_cast<TemporalScenarioViewModel *>(model.focusedViewModel())->presenter()->stateMachine();
    };

    QToolBar *bar = new QToolBar;
    // The tools
    m_scenarioToolActionGroup = new QActionGroup{bar};
    m_scenarioToolActionGroup->setEnabled(false);
    auto createtool = new QAction(tr("Create"), m_scenarioToolActionGroup);
    createtool->setCheckable(true);
    createtool->setData(QVariant::fromValue((int) ScenarioAction::Create));
    createtool->setShortcutContext(Qt::ApplicationShortcut);
    createtool->setShortcut(tr("Alt+c"));
    connect(createtool, &QAction::triggered, [=]()
    {
        if (focusedScenario())
                emit { focusedScenarioStateMachine().setCreateState(); }
    });

    auto movetool = new QAction(tr("Move"), m_scenarioToolActionGroup);
    movetool->setCheckable(true);
    movetool->setData(QVariant::fromValue((int) ScenarioAction::Move));
    movetool->setShortcutContext(Qt::ApplicationShortcut);
    movetool->setShortcut(tr("Alt+v"));
    connect(movetool, &QAction::triggered, [=]()
    {
        if (focusedScenario())
                emit { focusedScenarioStateMachine().setMoveState(); }
    });

    auto deckmovetool = new QAction(tr("Move Deck"), m_scenarioToolActionGroup);
    deckmovetool->setCheckable(true);
    deckmovetool->setData(QVariant::fromValue((int) ScenarioAction::DeckMove));
    deckmovetool->setShortcutContext(Qt::ApplicationShortcut);
    deckmovetool->setShortcut(tr("Alt+b"));
    connect(deckmovetool, &QAction::triggered, [=]()
    {
        if (focusedScenario())
                emit { focusedScenarioStateMachine().setDeckMoveState(); }
    });

    selecttool = new QAction(tr("Select"), m_scenarioToolActionGroup);
    selecttool->setCheckable(true);
    selecttool->setChecked(true);
    selecttool->setData(QVariant::fromValue((int) ScenarioAction::Select));
    selecttool->setShortcutContext(Qt::ApplicationShortcut);
    selecttool->setShortcut(tr("Alt+n"));
    connect(selecttool, &QAction::triggered, [=]()
    {
        if (focusedScenario())
                emit { focusedScenarioStateMachine().setSelectState(); }
    });

    // The action modes
    m_scenarioScaleModeActionGroup = new QActionGroup{bar};
    auto scale = new QAction(tr("Scale"), m_scenarioScaleModeActionGroup);
    scale->setCheckable(true);
    scale->setChecked(true);
    scale->setData(QVariant::fromValue(ExpandMode::Scale));
    scale->setShortcutContext(Qt::ApplicationShortcut);
    scale->setShortcut(tr("Alt+Shift+S"));
    connect(scale, &QAction::triggered, [=]()
    {
        if (focusedScenario())
                emit { focusedScenarioStateMachine().setScaleState(); }
    });


    auto grow = new QAction(tr("Grow/Shrink"), m_scenarioScaleModeActionGroup);
    grow->setCheckable(true);
    grow->setData(QVariant::fromValue(ExpandMode::Grow));
    grow->setShortcutContext(Qt::ApplicationShortcut);
    grow->setShortcut(tr("Alt+Shift+D"));
    connect(grow, &QAction::triggered, [=]()
    {
        if (focusedScenario())
                emit focusedScenarioStateMachine().setGrowState();
    });


    on_presenterChanged();

    bar->addActions(m_scenarioToolActionGroup->actions());
    bar->addSeparator();
    bar->addActions(m_scenarioScaleModeActionGroup->actions());

    return {bar};
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
        auto scenario = dynamic_cast<TemporalScenarioViewModel *>(model.focusedViewModel());
        m_scenarioToolActionGroup->setEnabled(scenario);
        m_scenarioScaleModeActionGroup->setEnabled(scenario);
        if (scenario)
        {
            // Set the current state on the statemachine.
            for (QAction *action : m_scenarioToolActionGroup->actions())
            {
                if (action->isChecked())
                {
                    switch (action->data().toInt())
                    {
                        case ScenarioAction::Create:
                            scenario->presenter()->stateMachine().setCreateState();
                            break;
                        case ScenarioAction::DeckMove:
                            scenario->presenter()->stateMachine().setDeckMoveState();
                            break;
                        case ScenarioAction::Move:
                            scenario->presenter()->stateMachine().setMoveState();
                            break;
                        case ScenarioAction::Select:
                            scenario->presenter()->stateMachine().setSelectState();
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
                            scenario->presenter()->stateMachine().setScaleState();
                            break;
                        case ExpandMode::Grow:
                            scenario->presenter()->stateMachine().setGrowState();
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

void ScenarioControl::on_documentChanged(Document *doc)
{
    on_presenterChanged();

    auto &model = IDocument::modelDelegate<BaseElementModel>(*currentDocument());
    auto onScenario = dynamic_cast<TemporalScenarioViewModel *>(model.focusedViewModel());

    selecttool->setChecked(true);
    m_scenarioToolActionGroup->setEnabled(onScenario);
    m_scenarioScaleModeActionGroup->setEnabled(onScenario);
}
