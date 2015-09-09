#include "ToolMenuActions.hpp"

#include "iscore/menu/MenuInterface.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Control/ScenarioControl.hpp"

#include <QKeyEvent>

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

    // SELECT AND MOVE
    m_selecttool = makeToolbarAction(
                     tr("Select and Move"),
                     m_scenarioToolActionGroup,
                     ScenarioToolKind::Select,
                     tr("Alt+x"));
    m_selecttool->setChecked(true);
    auto set_tool = [&] (ScenarioToolKind t) {
        if(auto&& pres = m_parent->focusedPresenter())
            pres->stateMachine().changeTool(static_cast<int>(t));
    };

    connect(m_selecttool, &QAction::toggled, this, [=](bool b) {
        if (b)
            set_tool(ScenarioToolKind::Select);
    });

    // CREATE
    m_createtool = makeToolbarAction(
                          tr("Create"),
                          m_scenarioToolActionGroup,
                          ScenarioToolKind::Create,
                          tr("Ctrl"));
    connect(m_createtool, &QAction::toggled, this, [=](bool b) {
        if(b)
            set_tool(ScenarioToolKind::Create);
    });

    // MOVEDECK
    auto slotmovetool = makeToolbarAction(
                            tr("Move Slot"),
                            m_scenarioToolActionGroup,
                            ScenarioToolKind::MoveSlot,
                            tr("Alt+b"));
    connect(slotmovetool, &QAction::triggered, this, [=]() {
        set_tool(ScenarioToolKind::MoveSlot);
    });

    // SHIFT
    m_shiftAction = makeToolbarAction(
                            tr("Fork Creation"),
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

    // SCALEMODE
    m_scenarioScaleModeActionGroup = new QActionGroup{this};

    auto scale = makeToolbarAction(
                     tr("Scale"),
                     m_scenarioScaleModeActionGroup,
                     ExpandMode::Scale,
                     tr("Alt+S"));
    scale->setChecked(true);
    connect(scale, &QAction::triggered, this, [=]()
    {
        m_parent->setExpandMode(ExpandMode::Scale);
    });

    auto grow = makeToolbarAction(
                    tr("Grow/Shrink"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Grow,
                    tr("Alt+D"));
    connect(grow, &QAction::triggered, this, [=]()
    {
        m_parent->setExpandMode(ExpandMode::Grow);
    });

    auto fixed = makeToolbarAction(
                    tr("Keep Duration"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Fixed,
                    tr("Alt+F"));
    connect(fixed, &QAction::triggered, this, [=]()
    {
        m_parent->setExpandMode(ExpandMode::Fixed);
    });

    connect(parent, &ScenarioControl::keyPressed,
            this,   &ToolMenuActions::keyPressed);

    connect(parent, &ScenarioControl::keyReleased,
            this,   &ToolMenuActions::keyReleased);

}

void ToolMenuActions::fillMenuBar(iscore::MenubarManager *menu)
{
    for(auto act : toolActions())
    {
        menu->insertActionIntoToplevelMenu(m_menuElt, act);
    }

    menu->addSeparatorIntoToplevelMenu(m_menuElt, iscore::ToolMenuElement::Separator_Tool);
    menu->insertActionIntoToplevelMenu(m_menuElt, m_shiftAction);
    menu->addSeparatorIntoToplevelMenu(m_menuElt, iscore::ToolMenuElement::Separator_Tool);
    for(auto act : modeActions())
    {
        menu->insertActionIntoToplevelMenu(m_menuElt, act);
    }
}

void ToolMenuActions::fillContextMenu(QMenu *menu, const Selection& sel)
{
    auto tool = menu->addMenu("Tool");
    tool->addActions(toolActions());
    tool->addAction(m_shiftAction);
    auto resize_mode = menu->addMenu("Resize mode");
    resize_mode->addActions(modeActions());
    m_scenarioToolActionGroup->setDisabled(false);
}

void ToolMenuActions::makeToolBar(QToolBar *bar)
{
    bar->addActions(toolActions());
    bar->addAction(m_shiftAction);
    bar->addSeparator();
    bar->addActions(modeActions());
}

void ToolMenuActions::setEnabled(bool arg)
{
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

