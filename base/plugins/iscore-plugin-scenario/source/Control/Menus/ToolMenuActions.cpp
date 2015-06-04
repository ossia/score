#include "ToolMenuActions.hpp"

#include "iscore/menu/MenuInterface.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"


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


ToolMenuActions::ToolMenuActions(iscore::ToplevelMenuElement menuElt, ScenarioControl* parent) :
    AbstractMenuActions{menuElt, parent}
{
    m_scenarioToolActionGroup = new QActionGroup{this};
    m_scenarioToolActionGroup->setDisabled(true);

    // NOTE : if a scenario isn't focused, they shouldn't event be clickable.
    m_selecttool = makeToolbarAction(
                     tr("Select"),
                     m_scenarioToolActionGroup,
                     Tool::Select,
                     tr("Alt+x"));
    m_selecttool->setChecked(true);
    connect(m_selecttool, &QAction::triggered, [=]()
    { m_parent->focusedPresenter()->stateMachine().changeTool(static_cast<int>(Tool::Select)); });

    auto createtool = makeToolbarAction(
                          tr("Create"),
                          m_scenarioToolActionGroup,
                          Tool::Create,
                          tr("Alt+c"));
    connect(createtool, &QAction::triggered, [=]()
    { m_parent->focusedPresenter()->stateMachine().changeTool(static_cast<int>(Tool::Create)); });

    auto movetool = makeToolbarAction(
                        tr("Move"),
                        m_scenarioToolActionGroup,
                        Tool::Move,
                        tr("Alt+v"));
    connect(movetool, &QAction::triggered, [=]()
    { m_parent->focusedPresenter()->stateMachine().changeTool(static_cast<int>(Tool::Move)); } );

    auto deckmovetool = makeToolbarAction(
                            tr("Move Deck"),
                            m_scenarioToolActionGroup,
                            Tool::MoveDeck,
                            tr("Alt+b"));
    connect(deckmovetool, &QAction::triggered, [=]()
    { m_parent->focusedPresenter()->stateMachine().changeTool(static_cast<int>(Tool::MoveDeck)); });


    m_scenarioScaleModeActionGroup = new QActionGroup{this};
    m_scenarioScaleModeActionGroup->setDisabled(true);

    auto scale = makeToolbarAction(
                     tr("Scale"),
                     m_scenarioScaleModeActionGroup,
                     ExpandMode::Scale,
                     tr("Alt+S"));
    scale->setChecked(true);
    connect(scale, &QAction::triggered, [=]()
    { m_parent->focusedPresenter()->stateMachine().setScaleState(); });

    auto grow = makeToolbarAction(
                    tr("Grow/Shrink"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Grow,
                    tr("Alt+D"));
    connect(grow, &QAction::triggered, [=]()
    { m_parent->focusedPresenter()->stateMachine().setGrowState(); });

    auto fixed = makeToolbarAction(
                    tr("Keep Duration"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Fixed,
                    tr("Alt+F"));
    connect(fixed, &QAction::triggered, [=]()
    { m_parent->focusedPresenter()->stateMachine().setFixedState(); });

}

void ToolMenuActions::fillMenuBar(iscore::MenubarManager *menu)
{
    for(auto act : toolActions())
    {
        menu->insertActionIntoToplevelMenu(m_menuElt, act);
    }
    menu->addSeparatorIntoToplevelMenu(m_menuElt, iscore::ToolMenuElement::Separator_Tool);
    for(auto act : modeActions())
    {
        menu->insertActionIntoToplevelMenu(m_menuElt, act);
    }
}

void ToolMenuActions::fillContextMenu(QMenu *menu)
{
    auto tool = menu->addMenu("Tool");
    tool->addActions(m_scenarioToolActionGroup->actions());
    auto resize_mode = menu->addMenu("Resize mode");
    resize_mode->addActions(m_scenarioScaleModeActionGroup->actions());
}

void ToolMenuActions::makeToolBar(QToolBar *bar)
{
    m_scenarioScaleModeActionGroup->setDisabled(true);
    m_scenarioToolActionGroup->setDisabled(true);
    bar->addActions(toolActions());
    bar->addSeparator();
    bar->addActions(modeActions());
}

void ToolMenuActions::setEnabled(bool arg)
{
    m_scenarioScaleModeActionGroup->setEnabled(arg);
    m_scenarioToolActionGroup->setEnabled(arg);
    if(arg)
    {
        m_selecttool->setChecked(true);
    }
}

QList<QAction *> ToolMenuActions::actions()
{
    QList<QAction*> list{};
    return list;
}

QList<QAction *> ToolMenuActions::modeActions()
{
    return m_scenarioScaleModeActionGroup->actions();
}

QList<QAction *> ToolMenuActions::toolActions()
{
    return m_scenarioToolActionGroup->actions();
}

