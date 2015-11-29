#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>
#include <QVariant>

#include <Process/ExpandMode.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Palette/Tool.hpp>
#include "ToolMenuActions.hpp"
#include <core/presenter/MenubarManager.hpp>

class QObject;
class TemporalScenarioPresenter;

template<typename Data>
QAction* makeToolbarAction(const QString& name,
                           QObject* parent,
                           const Data& data,
                           const QString& shortcut)
{
    auto act = new QAction{name, parent};
    act->setCheckable(true);
    act->setData(QVariant::fromValue((int) data));
    act->setShortcut(shortcut);
    act->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    act->setToolTip(name+ " (" + shortcut + ")");

    return act;
}


ToolMenuActions::ToolMenuActions(
        iscore::ToplevelMenuElement menuElt,
        ScenarioApplicationPlugin* parent) :
    ScenarioActions{menuElt, parent}
{
    m_scenarioToolActionGroup = new QActionGroup{this};
    m_scenarioToolActionGroup->setDisabled(true);

    // NOTE : if a scenario isn't focused, they shouldn't event be clickable.

    // SELECT AND MOVE
    m_selecttool = makeToolbarAction(
                     tr("Select and Move"),
                     m_scenarioToolActionGroup,
                     Scenario::Tool::Select,
                     tr("Alt+x"));
    m_selecttool->setObjectName("Select");
    m_selecttool->setChecked(true);

    connect(m_selecttool, &QAction::toggled, this, [=](bool b) {
        if (b)
            m_parent->editionSettings().setTool(Scenario::Tool::Select);
    });

    // CREATE
    m_createtool = makeToolbarAction(
                          tr("Create"),
                          m_scenarioToolActionGroup,
                          Scenario::Tool::Create,
                          tr("Ctrl"));
    connect(m_createtool, &QAction::toggled, this, [=](bool b) {
        if(b)
            m_parent->editionSettings().setTool(Scenario::Tool::Create);
    });

    // MOVEDECK
    auto slotmovetool = makeToolbarAction(
                            tr("Move Slot"),
                            m_scenarioToolActionGroup,
                            Scenario::Tool::MoveSlot,
                            tr("Alt+b"));
    connect(slotmovetool, &QAction::triggered, this, [=]() {
        m_parent->editionSettings().setTool(Scenario::Tool::MoveSlot);
    });

    // SHIFT
    m_shiftAction = makeToolbarAction(
                            tr("Sequence"),
                            this,
                            ExpandMode::Fixed,
                            tr("Shift"));
    connect(m_shiftAction, &QAction::toggled, this, [=] (bool val)
    {
        m_parent->editionSettings().setSequence(val);
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
        m_parent->editionSettings().setExpandMode(ExpandMode::Scale);
    });

    auto grow = makeToolbarAction(
                    tr("Grow/Shrink"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Grow,
                    tr("Alt+D"));
    connect(grow, &QAction::triggered, this, [=]()
    {
        m_parent->editionSettings().setExpandMode(ExpandMode::Grow);
    });

    auto fixed = makeToolbarAction(
                    tr("Keep Duration"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Fixed,
                    tr("Alt+F"));
    connect(fixed, &QAction::triggered, this, [=]()
    {
        m_parent->editionSettings().setExpandMode(ExpandMode::Fixed);
    });

    connect(parent, &ScenarioApplicationPlugin::keyPressed,
            this,   &ToolMenuActions::keyPressed);

    connect(parent, &ScenarioApplicationPlugin::keyReleased,
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

void ToolMenuActions::fillContextMenu(
        QMenu *menu,
        const Selection& sel,
        const TemporalScenarioPresenter& pres,
        const QPoint&,
        const QPointF&)
{
    auto tool = menu->addMenu("Tool");
    tool->addActions(toolActions());
    tool->addAction(m_shiftAction);
    auto resize_mode = menu->addMenu("Resize mode");
    resize_mode->addActions(modeActions());
    m_scenarioToolActionGroup->setDisabled(false);
}

bool ToolMenuActions::populateToolBar(QToolBar *bar)
{
    bar->addActions(toolActions());
    bar->addAction(m_shiftAction);
    bar->addSeparator();
    bar->addActions(modeActions());

    return true;
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

