#include <ossia/editor/state/state.hpp>
#include <QAction>
#include <QList>
#include <QMenu>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <qnamespace.h>

#include <QRect>
#include <QString>
#include <QVariant>
#include <algorithm>
#include <map>
#include <memory>

#include <Engine/Executor/ConstraintElement.hpp>
#include <Engine/Executor/EventElement.hpp>
#include <Engine/Executor/ScenarioElement.hpp>
#include <Engine/Executor/StateElement.hpp>
#include <Engine/Executor/StateElement.hpp>

#include "PlayContextMenu.hpp"
#include <ossia/editor/scenario/time_event.hpp>
#include <Process/LayerModel.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioRecordInitData.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <Scenario/Application/ScenarioRecordInitData.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iscore/actions/Menu.hpp>
#include <iscore/model/EntityMap.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/range/iterator_range.hpp>

namespace Engine
{
namespace Execution
{

using GraphNode = Scenario::TimeNodeModel*;

using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, GraphNode> ;
using AudioGraphVertice = decltype(boost::add_vertex(GraphNode{}, std::declval<Graph&>()));
struct ConstraintTimenodeGraphBuilder
{
  Graph graph;
  ConstraintTimenodeGraphBuilder(const Scenario::ScenarioInterface& scenar)
  {
    auto constraints = scenar.getConstraints();
    auto timenodes = scenar.getTimeNodes();
    iscore::hash_map<const Scenario::ConstraintModel*, int> cmap;
    iscore::hash_map<const Scenario::TimeNodeModel*, int> tmap;

    for(auto& tn : timenodes)
      tmap[&tn] = boost::add_vertex(&tn, graph);

    for(auto& cst : constraints)
    {
      boost::add_edge(tmap[&Scenario::startTimeNode(cst, scenar)], tmap[&Scenario::endTimeNode(cst, scenar)], graph);
    }

    auto get_name = [] (auto* elt) { return elt->metadata().getName().toStdString(); };

    std::stringstream s;
    boost::write_graphviz(s, graph, [&] (auto& out, const auto& v) {
        out << "[label=\"" << get_name(this->graph[v]) << "\"]";
    },
    [] (auto&&...) {});

    std::cout << s.str() << std::endl << std::flush;
  }

};

/**
 * @brief Sets the execution engine to play only the required parts.
 */
struct PlayFromConstraintScenarioPruner
{
  const Scenario::ScenarioInterface& scenar;
  Scenario::ConstraintModel& constraint;
  TimeValue time;

  void operator()(const Context& exec_ctx)
  {
    // We prune all the superfluous components of the scenario, ie the one that aren't either
    // the started constraint, or the ones following it.

    // First build a vector with all the constraints that we want to keep.

    // Then we add a constraint from the beginning of the scenario to this one,
    // and we do an offset.
    qDebug("yay");
  }

  std::vector<Id<Scenario::ConstraintModel>> constraintsToKeep()
  {
    // Start from our constraint.
    auto scenar_constraints = scenar.getConstraints();

    std::vector<Id<Scenario::ConstraintModel>> constraints;
    constraints.reserve(scenar_constraints.size());

    auto cur_cst = &constraint;
    Scenario::TimeNodeModel* cur_tn{};
    return constraints;


  }
};

PlayContextMenu::PlayContextMenu(
    ApplicationPlugin& plug, const iscore::GUIApplicationContext& ctx)
    : m_ctx{ctx}
{
  auto& exec_signals
      = m_ctx.components
            .applicationPlugin<Scenario::ScenarioApplicationPlugin>()
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
        auto ossia_state = Engine::iscore_to_ossia::state(*state, r_ctx);
        ossia_state.launch();
      }
    }
  });

  m_playConstraints = new QAction{tr("Play (Constraints)"), this};
  connect(m_playConstraints, &QAction::triggered, [&]() {
    const auto& ctx = m_ctx.documents.currentDocument()->context();
    auto sm = focusedScenarioInterface(ctx);
    if (!sm)
      return;

    for (auto& elt : sm->getConstraints())
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
    ConstraintTimenodeGraphBuilder g{scenar};
        const auto& ctx = m_ctx.documents.currentDocument()->context();
        auto& r_ctx
            = ctx.plugin<Engine::Execution::DocumentPlugin>().context();

        auto ossia_state
            = Engine::iscore_to_ossia::state(scenar.state(id), r_ctx);
        ossia_state.launch();
      });

  con(exec_signals, &Scenario::ScenarioExecution::playConstraint, this,
      [&](const Scenario::ScenarioInterface& scenar,
          const Id<ConstraintModel>& id) {
        plug.on_play(scenar.constraint(id), true);
      });

  con(exec_signals, &Scenario::ScenarioExecution::playFromConstraintAtDate, this,
      [&] (
      const Scenario::ScenarioInterface& scenar,
      Id<Scenario::ConstraintModel> id,
      const TimeValue& t) {

    // First we select the parent scenario of the constraint,
    auto& cst_to_play = scenar.constraint(id);

    // and the parent constraint of this scenario;
    // this is what needs executing.
    auto parent_constraint = safe_cast<Scenario::ConstraintModel*>(safe_cast<const QObject*>(&scenar)->parent());

    plug.on_play(*parent_constraint, true, PlayFromConstraintScenarioPruner{scenar, cst_to_play,t}, t);
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
    auto proc = safe_cast<Scenario::ProcessModel*>(
        &pres.layerModel().processModel());

    exec_signals.startRecording(
        *proc,
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
    auto proc = safe_cast<Scenario::ProcessModel*>(
        &pres.layerModel().processModel());

    exec_signals.startRecordingMessages(
        *proc,
        Scenario::ConvertToScenarioPoint(
            pres.view().mapFromScene(recdata.point),
            pres.zoomRatio(),
            pres.view().boundingRect().height()));

    m_recordAutomations->setData({});
  });

  auto& exec_ctx
      = m_ctx.applicationPlugin<ScenarioApplicationPlugin>()
            .execution();
  m_playFromHere = new QAction{tr("Play from here"), this};
  connect(m_playFromHere, &QAction::triggered, this, [&]() {
    exec_ctx.playAtDate(m_playFromHere->data().value<::TimeValue>());
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
      = ctxm.menu<ContextMenus::ConstraintContextMenu>();

  cst_cm.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx) {
        using namespace iscore;
        auto sel = ctx.context.selectionStack.currentSelection();
        if (sel.empty())
          return;

        if (ossia::any_of(sel, matches<Scenario::ConstraintModel>{}))
        {
          auto submenu = menu.findChild<QMenu*>("Constraint");
          ISCORE_ASSERT(submenu);

          submenu->addAction(m_playConstraints);
        }
      });

  state_cm.functions.push_back(
      [this](QMenu& menu, QPoint, QPointF, const Process::LayerContext& ctx) {
        using namespace iscore;
        auto sel = ctx.context.selectionStack.currentSelection();
        if (sel.empty())
          return;

        if (ossia::any_of(sel, matches<Scenario::StateModel>{}))
        {
          auto stateSubmenu = menu.findChild<QMenu*>("State");
          ISCORE_ASSERT(stateSubmenu);

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
