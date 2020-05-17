// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ToolMenuActions.hpp"

#include <Process/ExpandMode.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Palette/Tool.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/Menu.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QMenu>
#include <QString>
#include <QToolBar>
#include <QVariant>
#include <qnamespace.h>

class QObject;
namespace Scenario
{
class ScenarioPresenter;

template <typename Data>
QAction*
makeToolbarAction(const QString& name, QObject* parent, const Data& data, const QString& shortcut)
{
  auto act = new QAction{name, parent};

  act->setCheckable(true);
  act->setData(QVariant::fromValue((int)data));
  act->setShortcut(shortcut);
  act->setShortcutContext(Qt::WindowShortcut);
  act->setToolTip(name + " (" + shortcut + ")");

  return act;
}

ToolMenuActions::ToolMenuActions(ScenarioApplicationPlugin* parent) : m_parent{parent}
{
  if (!parent->context.applicationSettings.gui)
    return;
  m_scenarioToolActionGroup = new QActionGroup{this};

  // NOTE : if a scenario isn't focused, they shouldn't event be clickable.

  // SELECT AND MOVE
  m_selecttool = makeToolbarAction(
      tr("Select and Move"), m_scenarioToolActionGroup, Scenario::Tool::Select, tr("S"));
  m_selecttool->setShortcuts({Qt::Key_S, Qt::Key_M});
  m_selecttool->setToolTip({"Select and Move (S, M)"});
  m_selecttool->setObjectName("Select");
  m_selecttool->setChecked(true);

  setIcons(
      m_selecttool,
      QStringLiteral(":/icons/select_and_move_on.png"),
      QStringLiteral(":/icons/select_and_move_off.png"),
      QStringLiteral(":/icons/select_and_move_disabled.png"));

  connect(m_selecttool, &QAction::toggled, this, [=](bool b) {
    if (b && m_parent->editionSettings().tool() != Scenario::Tool::Select)
      m_parent->editionSettings().setTool(Scenario::Tool::Select);
  });

  // CREATE
  m_createtool
      = makeToolbarAction(tr("Create"), m_scenarioToolActionGroup, Scenario::Tool::Create, tr(""));

  setIcons(
      m_createtool,
      QStringLiteral(":/icons/create_on.png"),
      QStringLiteral(":/icons/create_off.png"),
      QStringLiteral(":/icons/create_disabled.png"));

  connect(m_createtool, &QAction::triggered, this, [=](bool b) {
    if (b && m_parent->editionSettings().tool() != Scenario::Tool::Create)
      m_parent->editionSettings().setTool(Scenario::Tool::Create);
  });

  // PLAY
  m_playtool
      = makeToolbarAction(tr("Play"), m_scenarioToolActionGroup, Scenario::Tool::Play, tr("P"));
  setIcons(
      m_playtool,
      QStringLiteral(":/icons/play_on.png"),
      QStringLiteral(":/icons/play_off.png"),
      QStringLiteral(":/icons/play_disabled.png"));

  connect(m_playtool, &QAction::triggered, this, [=](bool b) {
    if (b && m_parent->editionSettings().tool() != Scenario::Tool::Play)
      m_parent->editionSettings().setTool(Scenario::Tool::Play);
  });

  // MOVESLOT
  auto slotmovetool = makeToolbarAction(
      tr("Move Slot"), m_scenarioToolActionGroup, Scenario::Tool::MoveSlot, tr("Alt+b"));
  setIcons(
      slotmovetool,
      QStringLiteral(":/icons/move_on.png"),
      QStringLiteral(":/icons/move_off.png"),
      QStringLiteral(":/icons/move_disabled.png"));
  connect(slotmovetool, &QAction::triggered, this, [=]() {
    m_parent->editionSettings().setTool(Scenario::Tool::MoveSlot);
  });

  // SHIFT
  m_shiftAction = makeToolbarAction(tr("Sequence"), this, ExpandMode::CannotExpand, tr(""));
  setIcons(
      m_shiftAction,
      QStringLiteral(":/icons/sequence_on.png"),
      QStringLiteral(":/icons/sequence_off.png"),
      QStringLiteral(":/icons/sequence_disabled.png"));

  connect(m_shiftAction, &QAction::toggled, this, [=](bool val) {
    m_parent->editionSettings().setSequence(val);
  });

  // ALT
  m_altAction = makeToolbarAction(tr("Lock"), this, ExpandMode::CannotExpand, tr("Alt"));
  setIcons(
      m_altAction,
      QStringLiteral(":/icons/clip_duration_on.png"),
      QStringLiteral(":/icons/clip_duration_off.png"),
      QStringLiteral(":/icons/clip_duration_disabled.png"));

  connect(m_altAction, &QAction::toggled, this, [=](bool val) {
    m_parent->editionSettings().setLockMode(val ? LockMode::Constrained : LockMode::Free);
  });
  if (parent->context.mainWindow)
    parent->context.mainWindow->addAction(m_altAction);

  // SCALEMODE
  m_scenarioScaleModeActionGroup = new QActionGroup{this};

  m_scale = makeToolbarAction(
      tr("Scale"), m_scenarioScaleModeActionGroup, ExpandMode::Scale, tr("Alt+S"));
  m_scale->setChecked(true);

  setIcons(
      m_scale,
      QStringLiteral(":/icons/scale_on.png"),
      QStringLiteral(":/icons/scale_off.png"),
      QStringLiteral(":/icons/scale_disabled.png"));

  connect(m_scale, &QAction::triggered, this, [=]() {
    m_parent->editionSettings().setExpandMode(ExpandMode::Scale);
  });

  m_grow = makeToolbarAction(
      tr("Grow/Shrink"), m_scenarioScaleModeActionGroup, ExpandMode::GrowShrink, tr("Alt+D"));
  setIcons(
      m_grow,
      QStringLiteral(":/icons/grow_shrink_on.png"),
      QStringLiteral(":/icons/grow_shrink_off.png"),
      QStringLiteral(":/icons/grow_shrink_disabled.png"));

  connect(m_grow, &QAction::triggered, this, [=]() {
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

  connect(parent, &ScenarioApplicationPlugin::keyPressed, this, &ToolMenuActions::keyPressed);

  connect(parent, &ScenarioApplicationPlugin::keyReleased, this, &ToolMenuActions::keyReleased);

  con(parent->editionSettings(),
      &Scenario::EditionSettings::toolChanged,
      this,
      [=](Scenario::Tool t) {
        switch (t)
        {
          case Scenario::Tool::Create:
            if (!m_createtool->isChecked())
              m_createtool->setChecked(true);
            break;
          case Scenario::Tool::Play:
            if (!m_playtool->isChecked())
              m_playtool->setChecked(true);
            break;
          case Scenario::Tool::MoveSlot:
            if (!slotmovetool->isChecked())
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
            if (!m_selecttool->isChecked())
              m_selecttool->setChecked(true);
            break;
          default:
            break;
        }
      });

  con(parent->editionSettings(),
      &Scenario::EditionSettings::sequenceChanged,
      this,
      [=](bool sequence) {
        if (sequence)
        {
          if (!m_shiftAction->isChecked())
            m_shiftAction->setChecked(true);
        }
        else
        {
          if (m_shiftAction->isChecked())
            m_shiftAction->setChecked(false);
        }
      });

  con(parent->editionSettings(),
      &Scenario::EditionSettings::lockModeChanged,
      this,
      [=](LockMode lm) {
        if (lm)
        {
          if (!m_altAction->isChecked())
            m_altAction->setChecked(true);
        }
        else
        {
          if (m_altAction->isChecked())
            m_altAction->setChecked(false);
        }
      });

  con(parent->editionSettings(),
      &Scenario::EditionSettings::expandModeChanged,
      this,
      [=](ExpandMode mode) {
        switch (mode)
        {
          case ExpandMode::Scale:
            if (!m_scale->isChecked())
              m_scale->setChecked(true);
            break;
          case ExpandMode::GrowShrink:
          case ExpandMode::ForceGrow:
            if (!m_grow->isChecked())
              m_grow->setChecked(true);
            break;
          case ExpandMode::CannotExpand:
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

void ToolMenuActions::makeGUIElements(score::GUIElements& ref)
{
  auto& scenario_proc_cond
      = m_parent->context.actions
            .condition<Process::EnableWhenFocusedProcessIs<Scenario::ProcessModel>>();
  auto& scenario_iface_cond
      = m_parent->context.actions
            .condition<Process::EnableWhenFocusedProcessIs<Scenario::ScenarioInterface>>();
  auto& scenario_doc_cond
      = m_parent->context.actions.condition<score::EnableWhenDocumentIs<ScenarioDocumentModel>>();

  score::Menu& menu = m_parent->context.menus.get().at(score::Menus::Edit());

  // Tools
  {
    auto bar = new QToolBar{tr("Tools")};
    bar->addAction(m_selecttool);
    bar->addAction(m_createtool);
    bar->addAction(m_playtool);
    bar->addAction(m_altAction);

    ref.toolbars.emplace_back(bar, StringKey<score::Toolbar>("Tools"), Qt::TopToolBarArea, 800);

    menu.menu()->addSeparator();
    menu.menu()->addAction(m_selecttool);
    menu.menu()->addAction(m_createtool);
    menu.menu()->addAction(m_playtool);

    ref.actions.add<Actions::SelectTool>(m_selecttool);
    ref.actions.add<Actions::CreateTool>(m_createtool);
    ref.actions.add<Actions::PlayTool>(m_playtool);
    ref.actions.add<Actions::SequenceMode>(m_shiftAction);
    ref.actions.add<Actions::LockMode>(m_altAction);

    scenario_iface_cond.add<Actions::SelectTool>();
    scenario_iface_cond.add<Actions::PlayTool>();
    scenario_proc_cond.add<Actions::CreateTool>();
    scenario_proc_cond.add<Actions::SequenceMode>();
    scenario_proc_cond.add<Actions::LockMode>();
  }

  // Scale modes
  {
    auto bar = new QToolBar{tr("Modes")};
    bar->addAction(m_scale);
    bar->addAction(m_grow);

    ref.toolbars.emplace_back(bar, StringKey<score::Toolbar>("Modes"), Qt::TopToolBarArea, 900);

    menu.menu()->addSeparator();
    menu.menu()->addAction(m_scale);
    menu.menu()->addAction(m_grow);

    ref.actions.add<Actions::Scale>(m_scale);
    ref.actions.add<Actions::Grow>(m_grow);
    scenario_doc_cond.add<Actions::Scale>();
    scenario_doc_cond.add<Actions::Grow>();
  }
}

void ToolMenuActions::keyPressed(int key)
{
  if (key == Qt::Key_Shift)
  {
    m_shiftAction->setChecked(true);
  }
  else if (key == Qt::Key_Alt)
  {
    m_altAction->setChecked(true);
  }
}

void ToolMenuActions::keyReleased(int key)
{
  if (key == Qt::Key_Shift)
  {
    m_shiftAction->setChecked(false);
  }
  else if (key == Qt::Key_Alt)
  {
    m_altAction->setChecked(false);
  }
  m_selecttool->trigger();
}
}
