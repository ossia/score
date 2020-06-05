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
#include <QHBoxLayout>
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
makeToolbarAction(const QString& name, QObject* parent, const Data& data, const QKeySequence& shortcut)
{
  auto act = new QAction{name, parent};

  act->setCheckable(true);
  act->setData(QVariant::fromValue((int)data));
  act->setShortcut(shortcut);
  act->setShortcutContext(Qt::WindowShortcut);
  QString toolTip = name;
  QString sequence = shortcut.toString(QKeySequence::NativeText);
  if(!sequence.isEmpty())
  {
    toolTip += " (" + sequence + ")";
  }
  act->setToolTip(toolTip);

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
      QStringLiteral(":/icons/select_and_move_hover.png"),
      QStringLiteral(":/icons/select_and_move_off.png"),
      QStringLiteral(":/icons/select_and_move_disabled.png"));

  connect(m_selecttool, &QAction::toggled, this, [=](bool b) {
    if (b && m_parent->editionSettings().tool() != Scenario::Tool::Select)
      m_parent->editionSettings().setTool(Scenario::Tool::Select);
  });

  // CREATE
  m_createtool
      = makeToolbarAction(tr("Create"), m_scenarioToolActionGroup, Scenario::Tool::Create, tr("C"));

  setIcons(
      m_createtool,
      QStringLiteral(":/icons/create_on.png"),
      QStringLiteral(":/icons/create_hover.png"),
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
      QStringLiteral(":/icons/play_hover.png"),
      QStringLiteral(":/icons/play_off.png"),
      QStringLiteral(":/icons/play_disabled.png"));

  connect(m_playtool, &QAction::triggered, this, [=](bool b) {
    if (b && m_parent->editionSettings().tool() != Scenario::Tool::Play)
      m_parent->editionSettings().setTool(Scenario::Tool::Play);
  });

  m_lockAction = makeToolbarAction(tr("Lock"), this, ExpandMode::CannotExpand, {});
  m_lockAction->setToolTip(tr("Lock (Shift)"));
  setIcons(
      m_lockAction,
      QStringLiteral(":/icons/clip_duration_on.png"),
      QStringLiteral(":/icons/clip_duration_hover.png"),
      QStringLiteral(":/icons/clip_duration_off.png"),
      QStringLiteral(":/icons/clip_duration_disabled.png"));

  connect(m_lockAction, &QAction::toggled, this, [=](bool val) {
    m_parent->editionSettings().setLockMode(val ? LockMode::Constrained : LockMode::Free);
  });
  if (parent->context.mainWindow)
    parent->context.mainWindow->addAction(m_lockAction);

  // SCALEMODE
  m_scaleAction = makeToolbarAction(
        tr("Scale"), this, ExpandMode::Scale, {});
  m_scaleAction->setCheckable(true);
  m_scaleAction->setChecked(false);
  m_scaleAction->setToolTip(tr("Scale mode (Alt)"));

  connect(m_scaleAction, &QAction::toggled, this, [=] (bool b) {
    if(b)
      m_parent->editionSettings().setExpandMode(ExpandMode::Scale);
    else
      m_parent->editionSettings().setExpandMode(ExpandMode::GrowShrink);
  });

  if(auto mainw = parent->context.documentTabWidget)
  {
    mainw->addAction(m_lockAction);
    mainw->addAction(m_scaleAction);
  }

  connect(parent, &ScenarioApplicationPlugin::keyPressed, this, &ToolMenuActions::keyPressed);
  connect(parent, &ScenarioApplicationPlugin::keyReleased, this, &ToolMenuActions::keyReleased);

  con(parent->editionSettings(),
      &Scenario::EditionSettings::toolChanged,
      this,
      [=](Scenario::Tool t) {
        switch (t)
        {
          case Scenario::Tool::Create:
          case Scenario::Tool::CreateGraph:
          case Scenario::Tool::CreateSequence:
            if (!m_createtool->isChecked())
              m_createtool->setChecked(true);
            break;
          case Scenario::Tool::Play:
            if (!m_playtool->isChecked())
              m_playtool->setChecked(true);
            break;
          case Scenario::Tool::Disabled:
            break;
          case Scenario::Tool::Playing:
            m_createtool->setChecked(false);
            m_playtool->setChecked(false);
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
      &Scenario::EditionSettings::lockModeChanged,
      this,
      [=](LockMode lm) {
        if (lm)
        {
          if (!m_lockAction->isChecked())
            m_lockAction->setChecked(true);
        }
        else
        {
          if (m_lockAction->isChecked())
            m_lockAction->setChecked(false);
        }
      });

  con(parent->editionSettings(),
      &Scenario::EditionSettings::expandModeChanged,
      this,
      &ToolMenuActions::setExpandMode);

  setExpandMode(ExpandMode::GrowShrink); // Called here just to set the icons
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
    bar->addSeparator();
    bar->addAction(m_lockAction);
    bar->addAction(m_scaleAction);
    bar->setIconSize(QSize{24,24});

    ref.toolbars.emplace_back(bar, StringKey<score::Toolbar>("Tools"), Qt::TopToolBarArea, 800);

    menu.menu()->addSeparator();
    menu.menu()->addAction(m_selecttool);
    menu.menu()->addAction(m_createtool);
    menu.menu()->addAction(m_playtool);

    ref.actions.add<Actions::SelectTool>(m_selecttool);
    ref.actions.add<Actions::CreateTool>(m_createtool);
    ref.actions.add<Actions::PlayTool>(m_playtool);

    scenario_iface_cond.add<Actions::SelectTool>();
    scenario_iface_cond.add<Actions::PlayTool>();
    scenario_proc_cond.add<Actions::CreateTool>();
  }

  // Scale modes
  {
    menu.menu()->addSeparator();
    menu.menu()->addAction(m_lockAction);
    menu.menu()->addAction(m_scaleAction);

    ref.actions.add<Actions::LockMode>(m_lockAction);
    scenario_proc_cond.add<Actions::LockMode>();

    ref.actions.add<Actions::Scale>(m_scaleAction);
    scenario_doc_cond.add<Actions::Scale>();
  }
}

void ToolMenuActions::keyPressed(int key)
{
  if (key == Qt::Key_Shift)
    m_lockAction->setChecked(true);
  else if (key == Qt::Key_Control)
    m_scaleAction->setChecked(true);
}

void ToolMenuActions::keyReleased(int key)
{
  if (key == Qt::Key_Shift)
    m_lockAction->setChecked(false);
  else if (key == Qt::Key_Control)
    m_scaleAction->setChecked(false);
  m_selecttool->trigger();
}

void ToolMenuActions::setExpandMode(ExpandMode mode)
{
  switch (mode)
  {
    case ExpandMode::Scale:
    {
      if (!m_scaleAction->isChecked())
        m_scaleAction->setChecked(true);

      setIcons(
            m_scaleAction,
            QStringLiteral(":/icons/scale_content_on.png"),
            QStringLiteral(":/icons/scale_content_hover.png"),
            QStringLiteral(":/icons/scale_on.png"),
            QStringLiteral(":/icons/scale_content_disabled.png"));
      break;
    }
    case ExpandMode::GrowShrink:
    case ExpandMode::ForceGrow:
    {
      if (m_scaleAction->isChecked())
        m_scaleAction->setChecked(false);

      setIcons(
            m_scaleAction,
            QStringLiteral(":/icons/scale_content_on.png"),
            QStringLiteral(":/icons/scale_hover.png"),
            QStringLiteral(":/icons/scale_on.png"),
            QStringLiteral(":/icons/scale_disabled.png"));
      break;
    }
    case ExpandMode::CannotExpand:
      break;
      /*
    case ExpandMode::Fixed:
        if(!fixed->isChecked())
            fixed->setChecked(true);
        break;
        */
  }
}
}
