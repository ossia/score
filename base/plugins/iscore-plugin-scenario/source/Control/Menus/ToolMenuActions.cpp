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
    auto set_tool = [&] (Tool t) {
        if(auto&& pres = m_parent->focusedPresenter())
            pres->stateMachine().changeTool(static_cast<int>(t));
    };

    connect(m_selecttool, &QAction::toggled, this, [=](bool b) {
        if (b)
            set_tool(Tool::Select);
    });

    m_createtool = makeToolbarAction(
                          tr("Create"),
                          m_scenarioToolActionGroup,
                          Tool::Create,
                          tr("Alt+c"));
    connect(m_createtool, &QAction::toggled, this, [=](bool b) {
        if(b)
            set_tool(Tool::Create);
    });

    auto movetool = makeToolbarAction(
                        tr("Move"),
                        m_scenarioToolActionGroup,
                        Tool::Move,
                        tr("Alt+v"));
    connect(movetool, &QAction::triggered, this, [=]() {
        set_tool(Tool::Move);
    });

    auto deckmovetool = makeToolbarAction(
                            tr("Move Deck"),
                            m_scenarioToolActionGroup,
                            Tool::MoveDeck,
                            tr("Alt+b"));
    connect(deckmovetool, &QAction::triggered, this, [=]() {
        set_tool(Tool::MoveDeck);
    });


    m_shiftAction = makeToolbarAction(
                            tr("Vertical Move"),
                            this,
                            ExpandMode::Fixed,
                            tr("Shift"));
    connect(m_shiftAction, &QAction::toggled, this, [=] ()
    {
        if(m_parent->focusedPresenter())
        {
            auto& sm = m_parent->focusedPresenter()->stateMachine();

            if (m_shiftAction->isChecked())
                sm.shiftPressed();
            else
                sm.shiftReleased();
        }
    });

    m_scenarioScaleModeActionGroup = new QActionGroup{this};
    m_scenarioScaleModeActionGroup->setDisabled(true);

    auto scale = makeToolbarAction(
                     tr("Scale"),
                     m_scenarioScaleModeActionGroup,
                     ExpandMode::Scale,
                     tr("Alt+S"));
    scale->setChecked(true);
    connect(scale, &QAction::triggered, this, [=]()
    {
        if(auto&& pres = m_parent->focusedPresenter())
            pres->stateMachine().setScaleState();
    });

    auto grow = makeToolbarAction(
                    tr("Grow/Shrink"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Grow,
                    tr("Alt+D"));
    connect(grow, &QAction::triggered, this, [=]()
    {
        if(auto&& pres = m_parent->focusedPresenter())
            pres->stateMachine().setGrowState();
    });

    auto fixed = makeToolbarAction(
                    tr("Keep Duration"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Fixed,
                    tr("Alt+F"));
    connect(fixed, &QAction::triggered, this, [=]()
    {
        if(auto&& pres = m_parent->focusedPresenter())
            pres->stateMachine().setFixedState();
    });

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
    menu->addSeparatorIntoToplevelMenu(m_menuElt, iscore::ToolMenuElement::Separator_Tool);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_shiftAction);
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
    m_shiftAction->setDisabled(true);

    bar->addActions(toolActions());
    bar->addSeparator();
    bar->addActions(modeActions());
}

void ToolMenuActions::setEnabled(bool arg)
{
    m_scenarioScaleModeActionGroup->setEnabled(arg);
    m_scenarioToolActionGroup->setEnabled(arg);
    m_shiftAction->setEnabled(arg);
    if(arg)
    {
        m_selecttool->setChecked(true);
    }
}

void ToolMenuActions::keyPressed(int key)
{
    if (key == Qt::Key_Control)
    {
        m_createtool->setChecked(true);
    }
    if(key == Qt::Key_Shift)
    {
        m_shiftAction->setChecked(true);
    }
}

void ToolMenuActions::keyReleased(int key)
{
    if (key == Qt::Key_Control)
    {
        m_selecttool->setChecked(true);
    }
    if(key == Qt::Key_Shift)
    {
        m_shiftAction->setChecked(false);
    }
}

QList<QAction *> ToolMenuActions::actions()
{
    QList<QAction*> list{m_shiftAction};
    list += m_scenarioScaleModeActionGroup->actions();
    list += m_scenarioToolActionGroup->actions();

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

QAction *ToolMenuActions::shiftAction()
{
    return m_shiftAction;
}

