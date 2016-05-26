#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <qnamespace.h>

#include <QString>
#include <QToolBar>
#include <QVariant>

#include <iscore/menu/MenuInterface.hpp>
#include <Process/ExpandMode.hpp>
#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Palette/Tool.hpp>
#include "ToolMenuActions.hpp"
#include <Scenario/Application/ScenarioActions.hpp>
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
        ScenarioApplicationPlugin* parent) :
    m_parent{parent}
{
    m_scenarioToolActionGroup = new QActionGroup{this};

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

    setIcons(m_selecttool, QString(":/icons/select_and_move_on.png"), QString(":/icons/select_and_move_off.png"));

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

    setIcons(m_createtool, QString(":/icons/create_on.png"), QString(":/icons/create_off.png"));

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
    setIcons(m_playtool, QString(":/icons/play_on.png"), QString(":/icons/play_off.png"));

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
    setIcons(slotmovetool, QString(":/icons/move_on.png"), QString(":/icons/move_off.png"));
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

    m_scale = makeToolbarAction(
                     tr("Scale"),
                     m_scenarioScaleModeActionGroup,
                     ExpandMode::Scale,
                     tr("Alt+S"));
    m_scale->setChecked(true);

    setIcons(m_scale, QString(":/icons/scale_on.png"), QString(":/icons/scale_off.png"));

    connect(m_scale, &QAction::triggered, this, [=]()
    {
        m_parent->editionSettings().setExpandMode(ExpandMode::Scale);
    });

    m_grow = makeToolbarAction(
                    tr("Grow/Shrink"),
                    m_scenarioScaleModeActionGroup,
                    ExpandMode::GrowShrink,
                    tr("Alt+D"));
    setIcons(m_grow, QString(":/icons/grow_shrink_on.png"), QString(":/icons/grow_shrink_off.png"));

    connect(m_grow, &QAction::triggered, this, [=]()
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
                if(!m_scale->isChecked())
                    m_scale->setChecked(true);
                break;
            case ExpandMode::GrowShrink:
            case ExpandMode::ForceGrow:
                if(!m_grow->isChecked())
                    m_grow->setChecked(true);
                break;
            case ExpandMode::Fixed:
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

void ToolMenuActions::makeGUIElements(iscore::GUIElements& ref)
{
    auto& scenario_proc_cond = m_parent->context.actions.condition<Process::EnableWhenFocusedProcessIs<Scenario::ScenarioModel>>();
    auto& scenario_iface_cond = m_parent->context.actions.condition<Process::EnableWhenFocusedProcessIs<Scenario::ScenarioInterface>>();

    iscore::Menu& menu = m_parent->context.menus.get().at(iscore::Menus::Edit());

    // Tools
    {
        auto bar = new QToolBar{tr("Tools")};
        bar->addAction(m_selecttool);
        bar->addAction(m_createtool);
        bar->addAction(m_playtool);
        bar->addAction(m_shiftAction);

        ref.toolbars.emplace_back(bar, StringKey<iscore::Toolbar>("Tools"), 0, 1);

        menu.menu()->addSeparator();
        menu.menu()->addAction(m_selecttool);
        menu.menu()->addAction(m_createtool);
        menu.menu()->addAction(m_playtool);
        menu.menu()->addAction(m_shiftAction);

        ref.actions.add<Actions::SelectTool>(m_selecttool);
        ref.actions.add<Actions::CreateTool>(m_createtool);
        ref.actions.add<Actions::PlayTool>(m_playtool);
        ref.actions.add<Actions::SequenceMode>(m_shiftAction);

        scenario_iface_cond.add<Actions::SelectTool>();
        scenario_iface_cond.add<Actions::PlayTool>();
        scenario_proc_cond.add<Actions::CreateTool>();
        scenario_proc_cond.add<Actions::SequenceMode>();
    }

    // Scale modes
    {
        auto bar = new QToolBar{tr("Modes")};
        bar->addAction(m_scale);
        bar->addAction(m_grow);

        ref.toolbars.emplace_back(bar, StringKey<iscore::Toolbar>("Modes"), 0, 2);

        menu.menu()->addSeparator();
        menu.menu()->addAction(m_scale);
        menu.menu()->addAction(m_grow);

        ref.actions.add<Actions::Scale>(m_scale);
        ref.actions.add<Actions::Grow>(m_grow);
        scenario_iface_cond.add<Actions::Scale>();
        scenario_iface_cond.add<Actions::Grow>();
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

}
