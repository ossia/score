// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateActions.hpp"

#include <Process/Layer/LayerContextMenu.hpp>
#include <Process/ProcessContext.hpp>
#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Cohesion/RefreshStates.hpp>
#include <Scenario/Commands/Cohesion/SnapshotParameters.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/widgets/SetIcons.hpp>
namespace Scenario
{
StateActions::StateActions(ScenarioApplicationPlugin* parent)
    : m_parent{parent}
{
  if (!parent->context.applicationSettings.gui)
    return;
  m_refreshStates = new QAction{tr("Refresh states"), this};
  m_refreshStates->setShortcutContext(Qt::ApplicationShortcut);
  m_refreshStates->setShortcut(tr("Ctrl+U"));
  m_refreshStates->setToolTip(tr("Ctrl+U"));
  if (parent->context.mainWindow)
    parent->context.mainWindow->addAction(m_refreshStates);
  setIcons(
      m_refreshStates, QStringLiteral(":/icons/refresh_on.png"),
      QStringLiteral(":/icons/refresh_off.png"),
      QStringLiteral(":/icons/refresh_disabled.png"));

  connect(m_refreshStates, &QAction::triggered, this, [&]() {
    Command::RefreshStates(m_parent->currentDocument()->context());
  });

  m_snapshot = new QAction{this};
  m_snapshot->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  if (parent->context.mainWindow)
    parent->context.mainWindow->addAction(m_snapshot);

  setIcons(
      m_snapshot, QStringLiteral(":/icons/snapshot_on.png"),
      QStringLiteral(":/icons/snapshot_off.png"),
      QStringLiteral(":/icons/snapshot_disabled.png"));

  connect(m_snapshot, &QAction::triggered, this, [&]() {
    if (auto doc = m_parent->currentDocument())
      SnapshotParametersInStates(doc->context());
  });
  m_snapshot->setEnabled(false);
}

void StateActions::makeGUIElements(score::GUIElements& ref)
{
  using namespace score;

  Menu& object = m_parent->context.menus.get().at(Menus::Object());
  object.menu()->addAction(m_snapshot);
  object.menu()->addAction(m_refreshStates);

  Toolbar& tb = *ossia::find_if(ref.toolbars, [](auto& tb) {
    return tb.key() == StringKey<score::Toolbar>("Interval");
  });
  tb.toolbar()->addAction(m_snapshot);
  tb.toolbar()->addAction(m_refreshStates);

  ref.actions.add<Actions::Snapshot>(m_snapshot);
  ref.actions.add<Actions::RefreshStates>(m_refreshStates);

  auto& cond = m_parent->context.actions.condition<
      score::EnableWhenSelectionContains<Scenario::StateModel>>();
  cond.add<Actions::RefreshStates>();
  cond.add<Actions::Snapshot>();
}

void StateActions::setupContextMenu(Process::LayerContextMenuManager& ctxm)
{
  using namespace Process;
  Process::LayerContextMenu cm
      = MetaContextMenu<ContextMenus::StateContextMenu>::make();

  cm.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx) {
        using namespace score;
        auto sel = ctx.context.selectionStack.currentSelection();
        if (sel.empty())
          return;

        if (ossia::any_of(
                sel,
                matches<Scenario::StateModel>{})) // TODO : event or timesync ?
        {
          auto stateSubmenu = menu.addMenu(tr("State"));
          stateSubmenu->setObjectName("State");
          stateSubmenu->addAction(m_snapshot);
          stateSubmenu->addAction(m_refreshStates);
        }
      });

  ctxm.insert(std::move(cm));
}

CommandDispatcher<> StateActions::dispatcher()
{
  CommandDispatcher<> disp{
      m_parent->currentDocument()->context().commandStack};
  return disp;
}
}
