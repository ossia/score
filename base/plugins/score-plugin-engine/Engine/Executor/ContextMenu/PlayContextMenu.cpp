// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "PlayContextMenu.hpp"
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <Engine/Executor/IntervalComponent.hpp>
#include <Engine/Executor/EventComponent.hpp>
#include <Engine/Executor/ScenarioComponent.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Engine/score2OSSIA.hpp>

#include <Engine/Executor/ContextMenu/PlayFromIntervalInScenario.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioRecordInitData.hpp>
#include <Scenario/Application/ScenarioActions.hpp>

#include <core/presenter/DocumentManager.hpp>

namespace Engine
{
namespace Execution
{

PlayContextMenu::PlayContextMenu(
    ApplicationPlugin& plug, const score::GUIApplicationContext& ctx)
    : m_ctx{ctx}
{
  auto& exec_signals
      = m_ctx.guiApplicationPlugin<Scenario::ScenarioApplicationPlugin>()
            .execution();

  using namespace Scenario;

  /// Right-click actions
  m_playStates = new QAction{tr("Play (States)"), this};
  connect(m_playStates, &QAction::triggered, [=]() {
    const auto& ctx = m_ctx.documents.currentDocument()->context();
    auto sm = focusedScenarioInterface(ctx);
    if (sm)
    {
      auto& r_ctx = ctx.plugin<Engine::Execution::DocumentPlugin>().context();

      for (const StateModel* state : selectedElements(sm->getStates()))
      {
        auto ossia_state = Engine::score_to_ossia::state(*state, r_ctx);
        ossia_state.launch();
      }
    }
  });

  m_playIntervals = new QAction{tr("Play (Intervals)"), this};
  connect(m_playIntervals, &QAction::triggered, [&]() {
    const auto& ctx = m_ctx.documents.currentDocument()->context();
    auto sm = focusedScenarioInterface(ctx);
    if (!sm)
      return;

    for (auto& elt : sm->getIntervals())
    {
      if (elt.selection.get())
      {
        plug.on_play(elt, true);
        return;
      }
    }
  });
  m_playEvents = new QAction{tr("Play (Events)"), this};
  connect(m_playEvents, &QAction::triggered, [=] {
    /*
    if (auto sm = parent->focusedScenarioModel())
    {
        auto s_plugin = sm->findChild<OSSIAScenarioElement*>(QString(),
    Qt::FindDirectChildrenOnly);

        for(const auto& ev : selectedElements(sm->events))
        {
            s_plugin->events().at(ev->id())->OSSIAEvent()->happen();
        }
    }
    */
  });

  /// Play tool ///
  con(exec_signals, &Scenario::ScenarioExecution::playState, this,
      [=](const Scenario::ScenarioInterface& scenar,
          const Id<StateModel>& id) {
        const auto& ctx = m_ctx.documents.currentDocument()->context();
        auto& r_ctx
            = ctx.plugin<Engine::Execution::DocumentPlugin>().context();

        auto ossia_state
            = Engine::score_to_ossia::state(scenar.state(id), r_ctx);
        ossia_state.launch();
      });

  con(exec_signals, &Scenario::ScenarioExecution::playInterval, this,
      [&](const Scenario::ScenarioInterface& scenar,
          const Id<IntervalModel>& id) {
        plug.on_play(scenar.interval(id), true);
      });

  con(exec_signals, &Scenario::ScenarioExecution::playFromIntervalAtDate, this,
      [&] (
      const Scenario::ScenarioInterface& scenar,
      Id<Scenario::IntervalModel> id,
      const TimeVal& t) {

    // First we select the parent scenario of the interval,
    auto& cst_to_play = scenar.interval(id);

    // and the parent interval of this scenario;
    // this is what needs executing.
    auto parent_interval =
        safe_cast<Scenario::IntervalModel*>(
          dynamic_cast<const QObject*>(&scenar)->parent());

    // We start playing the parent scenario.
    // TODO: this also plays the other processes of the interval? Maybe remove them, too ?
    plug.on_play(
          *parent_interval,
          true,
          PlayFromIntervalScenarioPruner{scenar, cst_to_play, t},
          t);
  });


  /// Record signals ///
  m_recordAutomations = new QAction{tr("Record automations from here"), this};
  connect(m_recordAutomations, &QAction::triggered, [=, &exec_signals]() {
    const auto& recdata
        = m_recordAutomations->data().value<ScenarioRecordInitData>();
    if (!recdata.presenter)
      return;

    auto& pres
        = *safe_cast<const TemporalScenarioPresenter*>(recdata.presenter);
    auto proc = safe_cast<const Scenario::ProcessModel*>(&pres.model());
    auto p = const_cast<Scenario::ProcessModel*>(proc);

    exec_signals.startRecording(
        *p,
        Scenario::ConvertToScenarioPoint(
            pres.view().mapFromScene(recdata.point),
            pres.zoomRatio(),
            pres.view().boundingRect().height()));

    m_recordAutomations->setData({});
  });

  m_recordMessages = new QAction{tr("Record messages from here"), this};
  connect(m_recordMessages, &QAction::triggered, [=, &exec_signals]() {
    const auto& recdata
        = m_recordMessages->data().value<ScenarioRecordInitData>();
    if (!recdata.presenter)
      return;

    auto& pres
        = *safe_cast<const TemporalScenarioPresenter*>(recdata.presenter);
    auto proc = safe_cast<const Scenario::ProcessModel*>(&pres.model());
    auto p = const_cast<Scenario::ProcessModel*>(proc);

    exec_signals.startRecordingMessages(
        *p,
        Scenario::ConvertToScenarioPoint(
            pres.view().mapFromScene(recdata.point),
            pres.zoomRatio(),
            pres.view().boundingRect().height()));

    m_recordAutomations->setData({});
  });

  auto& exec_ctx
      = m_ctx.guiApplicationPlugin<ScenarioApplicationPlugin>()
            .execution();
  m_playFromHere = new QAction{tr("Play from here"), this};
  connect(m_playFromHere, &QAction::triggered, this, [&]() {
    exec_ctx.playAtDate(m_playFromHere->data().value<::TimeVal>());
  });
}

void PlayContextMenu::setupContextMenu(Process::LayerContextMenuManager& ctxm)
{
  using namespace Process;
  Process::LayerContextMenu& scenario_cm
      = ctxm.menu<ContextMenus::ScenarioModelContextMenu>();
  Process::LayerContextMenu& state_cm
      = ctxm.menu<ContextMenus::StateContextMenu>();
  Process::LayerContextMenu& cst_cm
      = ctxm.menu<ContextMenus::IntervalContextMenu>();

  cst_cm.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx) {
        using namespace score;
        auto sel = ctx.context.selectionStack.currentSelection();
        if (sel.empty())
          return;

        if (ossia::any_of(sel, matches<Scenario::IntervalModel>{}))
        {
          auto submenu = menu.findChild<QMenu*>("Interval");
          if(submenu)
            submenu->addAction(m_playIntervals);
        }
      });

  state_cm.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx) {
        using namespace score;
        auto sel = ctx.context.selectionStack.currentSelection();
        if (sel.empty())
          return;

        if (ossia::any_of(sel, matches<Scenario::StateModel>{}))
        {
          auto stateSubmenu = menu.findChild<QMenu*>("State");
          SCORE_ASSERT(stateSubmenu);

          stateSubmenu->addAction(m_playStates);
        }
      });

  scenario_cm.functions.push_back([this](
      QMenu& menu, QPoint, QPointF scenept, const Process::LayerContext& ctx) {
    auto& pres
        = safe_cast<Scenario::TemporalScenarioPresenter&>(ctx.presenter);
    auto scenPoint = Scenario::ConvertToScenarioPoint(
        scenept, pres.zoomRatio(), pres.view().height());
    m_playFromHere->setData(QVariant::fromValue(scenPoint.date));
    menu.addAction(m_playFromHere);

    auto sel = ctx.context.selectionStack.currentSelection();
    if (!sel.empty())
      return;

    menu.addAction(m_recordAutomations);
    menu.addAction(m_recordMessages);

    auto data = QVariant::fromValue(
        Scenario::ScenarioRecordInitData{&pres, scenept});
    m_recordAutomations->setData(data);
    m_recordMessages->setData(data);

  });
}
}
}
