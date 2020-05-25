// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "IntervalActions.hpp"

#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Commands/Cohesion/CreateCurves.hpp>
#include <Scenario/Commands/Interval/AddProcessToInterval.hpp>
#include <Scenario/Commands/Scenario/HideRackInViewModel.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <Scenario/DialogWidget/AddProcessDialog.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioPresenter.hpp>
#include <Scenario/Process/ScenarioView.hpp>

#include <score/actions/ActionManager.hpp>
#include <score/actions/MenuManager.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/selection/Selectable.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QToolBar>
namespace Scenario
{
// TODO you're better than this
auto selectedIntervalsInCurrentDocument(const score::GUIApplicationContext& appContext);
auto selectedIntervalsInCurrentDocument(const score::GUIApplicationContext& appContext)
{
  auto sel = appContext.documents.currentDocument()->selectionStack().currentSelection();
  QList<const Scenario::IntervalModel*> selected_elements;
  for (auto obj : sel)
  {
    if (auto casted_obj = dynamic_cast<const Scenario::IntervalModel*>(obj.data()))
    {
      selected_elements.push_back(casted_obj);
    }
  }

  return selected_elements;
}

IntervalActions::IntervalActions(ScenarioApplicationPlugin* parent) : m_parent{parent}
{
  if (!parent->context.applicationSettings.gui)
    return;
  const auto& appContext = parent->context;

  m_addProcess = new QAction{tr("Add Process in interval"), this};
  connect(m_addProcess, &QAction::triggered, [&]() {
    if (selectedIntervalsInCurrentDocument(appContext).isEmpty())
      return;

    auto& fact = appContext.interfaces<Process::ProcessFactoryList>();
    auto dialog = new AddProcessDialog{
        fact, Process::ProcessFlags::SupportsTemporal, qApp->activeWindow()};

    dialog->on_okPressed = [this](auto k, const QString& data) { addProcessInInterval(k, data); };
    dialog->launchWindow();
    dialog->deleteLater();
  });

  m_showRacks = new QAction{tr("Show Racks"), this};
  m_showRacks->setShortcutContext(Qt::ApplicationShortcut);
  m_showRacks->setToolTip(tr("Show racks"));
  connect(m_showRacks, &QAction::triggered, this, &IntervalActions::on_showRacks);

  m_hideRacks = new QAction{tr("Hide Racks"), this};
  m_hideRacks->setShortcutContext(Qt::ApplicationShortcut);
  m_hideRacks->setToolTip(tr("Hide racks"));
  connect(m_hideRacks, &QAction::triggered, this, &IntervalActions::on_hideRacks);
}

IntervalActions::~IntervalActions() { }

void IntervalActions::makeGUIElements(score::GUIElements& ref)
{
  using namespace score;

  Menu& object = m_parent->context.menus.get().at(Menus::Object());
  object.menu()->addAction(m_addProcess);
  object.menu()->addAction(m_showRacks);
  object.menu()->addAction(m_hideRacks);

  ref.actions.add<Actions::AddProcess>(m_addProcess);
  ref.actions.add<Actions::ShowRacks>(m_showRacks);
  ref.actions.add<Actions::HideRacks>(m_hideRacks);

  auto& cond = m_parent->context.actions
                   .condition<score::EnableWhenSelectionContains<Scenario::IntervalModel>>();
  cond.add<Actions::AddProcess>();
  cond.add<Actions::ShowRacks>();
  cond.add<Actions::HideRacks>();
}

void IntervalActions::setupContextMenu(Process::LayerContextMenuManager& ctxm)
{
  using namespace Process;
  Process::LayerContextMenu cm = MetaContextMenu<ContextMenus::IntervalContextMenu>::make();

  cm.functions.push_back([this](QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx) {
    using namespace score;
    auto sel = ctx.context.selectionStack.currentSelection();
    if (sel.empty())
      return;

    auto selectedIntervals = filterSelectionByType<IntervalModel>(sel);

    if (selectedIntervals.size() >= 1)
    {
      auto cstrSubmenu = menu.addMenu(tr("Interval"));
      cstrSubmenu->setObjectName("Interval");
      if (m_addProcess)
      {
        cstrSubmenu->addAction(m_addProcess);
      }
      cstrSubmenu->addAction(m_showRacks);
      cstrSubmenu->addAction(m_hideRacks);
    }
  });

  ctxm.insert(std::move(cm));
}

void IntervalActions::addProcessInInterval(
    const UuidKey<Process::ProcessModel>& processName,
    const QString& data)
{
  auto selectedIntervals = selectedIntervalsInCurrentDocument(m_parent->context);
  if (selectedIntervals.isEmpty())
    return;

  auto cmd = new Scenario::Command::AddProcessToInterval(
      **selectedIntervals.begin(), processName, data, {});

  dispatcher().submit(cmd);
}

void IntervalActions::on_showRacks()
{
  if (auto doc = m_parent->currentDocument())
  {
    auto selected_intervals
        = filterSelectionByType<IntervalModel>(doc->context().selectionStack.currentSelection());
    for (const IntervalModel* c : selected_intervals)
    {
      if (!c->processes.empty())
        const_cast<IntervalModel*>(c)->setSmallViewVisible(true);
    }
  }
}

void IntervalActions::on_hideRacks()
{
  if (auto doc = m_parent->currentDocument())
  {
    auto selected_intervals
        = filterSelectionByType<IntervalModel>(doc->context().selectionStack.currentSelection());
    for (const IntervalModel* c : selected_intervals)
    {
      if (!c->processes.empty())
        const_cast<IntervalModel*>(c)->setSmallViewVisible(false);
    }
  }
}

CommandDispatcher<> IntervalActions::dispatcher()
{
  CommandDispatcher<> disp{m_parent->currentDocument()->context().commandStack};
  return disp;
}
}
