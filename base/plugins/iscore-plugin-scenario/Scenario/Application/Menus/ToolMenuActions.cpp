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

#include <iscore/widgets/SetIcons.hpp>

class QObject;
namespace Scenario
{
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
    act->setShortcutContext(Qt::WindowShortcut);
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
                     tr("S"));
    m_selecttool->setShortcuts({Qt::Key_S, Qt::Key_M});
    m_selecttool->setToolTip({"Select and Move (S, M)"});
    m_selecttool->setObjectName("Select");
    m_selecttool->setChecked(true);

    setIcons(m_selecttool, QString(":/icones/select_and_move_on.png"), QString(":/icones/select_and_move_off.png"));

    connect(m_selecttool, &QAction::toggled, this, [=](bool b) {
        if (b)
            m_parent->editionSettings().setTool(Scenario::Tool::Select);
    });

    // CREATE
    m_createtool = makeToolbarAction(
                          tr("Create"),
                          m_scenarioToolActionGroup,
                          Scenario::Tool::Create,
                          tr("C"));

    setIcons(m_createtool, QString(":/icones/create_on.png"), QString(":/icones/create_off.png"));

    connect(m_createtool, &QAction::triggered, this, [=](bool b) {
        if(b)
            m_parent->editionSettings().setTool(Scenario::Tool::Create);
    });

    // PLAY
    m_playtool = makeToolbarAction(
                     tr("Play"),
                     m_scenarioToolActionGroup,
                     Scenario::Tool::Play,
                     tr("P"));
    setIcons(m_playtool, QString(":/icones/play_on.png"), QString(":/icones/play_off.png"));

    connect(m_playtool, &QAction::triggered, this, [=] (bool b) {
        if(b)
            m_parent->editionSettings().setTool(Scenario::Tool::Play);
    });

    // MOVESLOT
    auto slotmovetool = makeToolbarAction(
                            tr("Move Slot"),
                            m_scenarioToolActionGroup,
                            Scenario::Tool::MoveSlot,
                            tr("Alt+b"));
    setIcons(slotmovetool, QString(":/icones/move_on.png"), QString(":/icones/move_off.png"));
    connect(slotmovetool, &QAction::triggered, this, [=]() {
        m_parent->editionSettings().setTool(Scenario::Tool::MoveSlot);
    });

    // SHIFT
    m_shiftAction = makeToolbarAction(
                            tr("Sequence"),
                            this,
                            ExpandMode::Fixed,
                            tr("Shift"));
    setIcons(m_shiftAction, QString(":/icones/sequence_on.png"), QString(":/icones/sequence_off.png"));

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

    setIcons(scale, QString(":/icones/scale_on.png"), QString(":/icones/scale_off.png"));

    connect(scale, &QAction::triggered, this, [=]()
    {
        m_parent->editionSettings().setExpandMode(ExpandMode::Scale);
    });

    auto grow = makeToolbarAction(
                    tr("Grow/Shrink"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::GrowShrink,
                    tr("Alt+D"));
    setIcons(grow, QString(":/icones/grow_shrink_on.png"), QString(":/icones/grow_shrink_off.png"));

    connect(grow, &QAction::triggered, this, [=]()
    {
        m_parent->editionSettings().setExpandMode(ExpandMode::GrowShrink);
    });

    /*
    auto fixed = makeToolbarAction(
                    tr("Keep Duration"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::Fixed,
                    tr("Alt+F"));
    connect(fixed, &QAction::triggered, this, [=]()
    {
        m_parent->editionSettings().setExpandMode(ExpandMode::Fixed);
    });
    */

    connect(parent, &ScenarioApplicationPlugin::keyPressed,
            this,   &ToolMenuActions::keyPressed);

    connect(parent, &ScenarioApplicationPlugin::keyReleased,
            this,   &ToolMenuActions::keyReleased);

    con(parent->editionSettings(), &Scenario::EditionSettings::toolChanged,
        this, [=] (Scenario::Tool t) {
        switch(t)
        {
            case Scenario::Tool::Create:
                if(!m_createtool->isChecked())
                    m_createtool->setChecked(true);
                break;
            case Scenario::Tool::Play:
                if(!m_playtool->isChecked())
                    m_playtool->setChecked(true);
                break;
            case Scenario::Tool::MoveSlot:
                if(!slotmovetool->isChecked())
                    slotmovetool->setChecked(true);
                break;
            case Scenario::Tool::Disabled:
                break;
            case Scenario::Tool::Playing:
                m_createtool->setChecked(false);
                m_playtool->setChecked(false);
                slotmovetool->setChecked(false);
                m_selecttool->setChecked(false);
                break;
            case Scenario::Tool::Select:
                if(!m_selecttool->isChecked())
                    m_selecttool->setChecked(true);
                break;
            default:
                break;
        }
    });

    con(parent->editionSettings(), &Scenario::EditionSettings::sequenceChanged,
        this, [=] (bool sequence) {
        if(sequence)
        {
            if(!m_shiftAction->isChecked())
                m_shiftAction->setChecked(true);
        }
        else
        {
            if(m_shiftAction->isChecked())
                m_shiftAction->setChecked(false);
        }
    });

    con(parent->editionSettings(), &Scenario::EditionSettings::expandModeChanged,
        this, [=] (ExpandMode mode)
    {
        switch(mode)
        {
            case ExpandMode::Scale:
                if(!scale->isChecked())
                    scale->setChecked(true);
                break;
            case ExpandMode::GrowShrink:
            case ExpandMode::ForceGrow:
                if(!grow->isChecked())
                    grow->setChecked(true);
                break;
                /*
            case ExpandMode::Fixed:
                if(!fixed->isChecked())
                    fixed->setChecked(true);
                break;
                */

        }
    });
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
    if(key == Qt::Key_Shift)
    {
        m_shiftAction->setChecked(true);
    }
}

void ToolMenuActions::keyReleased(int key)
{
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
}
