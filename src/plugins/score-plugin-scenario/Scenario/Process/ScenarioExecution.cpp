// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/ExecutionChecker/CSPCoherencyCheckerInterface.hpp>
#include <Scenario/ExecutionChecker/CSPCoherencyCheckerList.hpp>
#include <Scenario/ExecutionChecker/CoherencyCheckerFactoryInterface.hpp>
#include <Scenario/Process/ScenarioExecImpl.hpp>
#include <Scenario/Process/ScenarioExecution.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/editor/loop/loop.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_sync.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state.hpp>

#include <wobjectimpl.h>

#include <vector>
W_OBJECT_IMPL(Execution::ScenarioComponentBase)

namespace Execution
{
ScenarioComponentBase::ScenarioComponentBase(
    Scenario::ProcessModel& element, const Context& ctx, QObject* parent)
    : ProcessComponent_T<
        Scenario::ProcessModel,
        ossia::scenario>{element, ctx, "ScenarioComponent", nullptr}
    , m_ctx{ctx}
    , m_graph{element}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  this->setObjectName("OSSIAScenarioElement");

  if(element.hasCycles())
  {
    score::warning(
        nullptr, "Warning !",
        "A scenario has cycles. It won't be executing. Look for the red "
        "transitions.");
    throw std::runtime_error("Processus cannot execute");
  }

  // Setup of the OSSIA API Part
  m_ossia_process = std::make_shared<ossia::scenario>();

  node = m_ossia_process->node;
  ((ossia::nodes::forward_node*)this->node.get())
      ->audio_in.sources.reserve(element.intervals.size() * 2);

  connect(
      this, &ScenarioComponentBase::sig_eventCallback, this,
      &ScenarioComponentBase::eventCallback, Qt::QueuedConnection);
}

ScenarioComponentBase::~ScenarioComponentBase()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
}

void ScenarioComponentBase::playInterval(const Scenario::IntervalModel& itv)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  if(auto comp = this->m_ossia_intervals.find(itv.id());
     comp != this->m_ossia_intervals.end())
  {
    auto& c = comp->second;
    std::shared_ptr<ossia::scenario> proc
        = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);
    auto& ossia_c = c->OSSIAInterval();

    ossia::musical_sync quantRate = itv.quantizationRate();
    if(quantRate < 0)
    {
      auto parent_metrics = Scenario::closestParentWithMusicalMetrics(&itv);
      if(parent_metrics.parent)
      {
        quantRate = parent_metrics.parent->quantizationRate();
      }
      else if(parent_metrics.lastFound)
      {
        // At worst we use the root interval's quantization rate
        quantRate = parent_metrics.lastFound->quantizationRate();
      }
      if(quantRate < 0)
      {
        quantRate = 0.;
      }
    }

    in_exec([proc, ossia_c, rate = quantRate] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
      // FIXME this is incorrect for sub-scenarios.
      // We have to adjust with parent_metrics.delta !
      proc->request_start_interval(*ossia_c, rate);
    });

    startIntervalExecution(itv.id());
  }
}

void ScenarioComponentBase::stopInterval(const Scenario::IntervalModel& itv)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  if(auto comp = this->m_ossia_intervals.find(itv.id());
     comp != this->m_ossia_intervals.end())
  {
    auto& c = comp->second;
    std::shared_ptr<ossia::scenario> proc
        = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);
    auto& ossia_c = c->OSSIAInterval();

    ossia::musical_sync quantRate = itv.quantizationRate();
    if(quantRate < 0)
    {
      auto parent_metrics = Scenario::closestParentWithMusicalMetrics(&itv);
      if(parent_metrics.parent)
      {
        quantRate = parent_metrics.parent->quantizationRate();
      }
      else if(parent_metrics.lastFound)
      {
        // At worst we use the root interval's quantization rate
        quantRate = parent_metrics.lastFound->quantizationRate();
      }
      if(quantRate < 0)
      {
        quantRate = 0.;
      }
    }

    in_exec([proc, ossia_c, rate = quantRate] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
      // FIXME this is incorrect for sub-scenarios.
      // We have to adjust with parent_metrics.delta !
      proc->request_stop_interval(*ossia_c, rate);
    });

    stopIntervalExecution(itv.id());
  }
}

ScenarioComponent::ScenarioComponent(
    Scenario::ProcessModel& proc, const Context& ctx, QObject* parent)
    : ScenarioComponentHierarchy{score::lazy_init_t{}, proc, ctx, parent}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
}

void ScenarioComponent::lazy_init()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  // set-up the ports
  const Context& ctx = system();
  auto ossia_sc = std::dynamic_pointer_cast<ossia::scenario>(m_ossia_process);

  ScenarioComponentHierarchy::init();

  if(auto fact = ctx.doc.app.interfaces<Scenario::CSPCoherencyCheckerList>().get())
  {
    m_checker = fact->make(process(), ctx.doc.app, m_properties);
    if(m_checker)
    {
      m_properties.timesyncs[Id<Scenario::TimeSyncModel>(0)].date = 0;
      m_checker->computeDisplacement(m_pastTn, m_properties);
    }
  }

  ossia_sc->set_exclusive(process().exclusive());
  connect(
      &process(), &Scenario::ProcessModel::exclusiveChanged, this,
      [ossia_sc = std::weak_ptr(ossia_sc)](bool ex) {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    if(auto sc = ossia_sc.lock())
      sc->set_exclusive(ex);
  });

  /* TODO reinstate settings
  if (ctx.doc.app.settings<Settings::Model>().getScoreOrder())
  {
    std::vector<ossia::edge_ptr> edges_to_add;
    edges_to_add.reserve(m_ossia_intervals.size() * 2);

    for (auto& state : m_ossia_states)
    {
      // Add link between previous interval and state & first state process
      // And between state's last process and interval's first process
      auto& state_model = state.second->state();
      auto prev = state_model.previousInterval();
      auto next = state_model.nextInterval();
      auto& state_procs = state.second->processes();
      std::shared_ptr<ossia::graph_node> state_first_node, state_last_node;

      if (state.second->node())
      {
        state_first_node = state.second->node();

        if (state_procs.empty())
          state_last_node = state.second->node();
      }

      if (!state_procs.empty())
      {
        if (!state_first_node)
        {
          state_first_node
              = state_procs.at(state_model.stateProcesses.begin()->id())
                    ->OSSIAProcess()
                    .node;
        }

        state_last_node
            = state_procs.at(state_model.stateProcesses.rbegin()->id())
                  ->OSSIAProcess()
                  .node;
      }

      if (!state_first_node || !state_last_node)
        continue;

      if (prev)
      {
        auto prev_itv = m_ossia_intervals.at(*prev);
        SCORE_ASSERT(prev_itv->OSSIAInterval()->node);

        edges_to_add.push_back(ossia::make_edge(
            ossia::dependency_connection{}, ossia::outlet_ptr{},
            ossia::inlet_ptr{}, prev_itv->OSSIAInterval()->node,
            state_first_node));
      }

      if (next)
      {
        auto next_itv = m_ossia_intervals.at(*next);
        SCORE_ASSERT(next_itv->OSSIAInterval()->node);
        if (!next_itv->processes().empty())
        {
          auto proc = next_itv->processes().at(
              next_itv->interval().processes.begin()->id());
          if (auto ossia_proc = proc->OSSIAProcessPtr())
          {
            if (auto node = ossia_proc->node)
            {
              edges_to_add.push_back(ossia::make_edge(
                  ossia::dependency_connection{}, ossia::outlet_ptr{},
                  ossia::inlet_ptr{}, state_last_node, node));
            }
          }
        }
        else
        {
          edges_to_add.push_back(ossia::make_edge(
              ossia::dependency_connection{}, ossia::outlet_ptr{},
              ossia::inlet_ptr{}, state_last_node,
              next_itv->OSSIAInterval()->node));
        }
      }
    }

    std::weak_ptr<ossia::graph_interface> g_weak = ctx.execGraph;

    in_exec([edges = std::move(edges_to_add), g_weak] {
      if (auto g = g_weak.lock())
      {
        for (auto& c : edges)
        {
          g->connect(std::move(c));
        }
      }
    });
  }
  */
}

void ScenarioComponent::cleanup()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  clear();
  ProcessComponent::cleanup();
}

void ScenarioComponentBase::stop()
{
  ScenarioExecImpl::stop_body(*this);
  ProcessComponent::stop();
}

std::function<void()>
ScenarioComponentBase::removing(const Scenario::IntervalModel& e, IntervalComponent& c)
{
  return ScenarioExecImpl::removing_interval(*this, e, c);
}

std::function<void()>
ScenarioComponentBase::removing(const Scenario::TimeSyncModel& e, TimeSyncComponent& c)
{
  return ScenarioExecImpl::removing_timesync(*this, e, c);
}

std::function<void()>
ScenarioComponentBase::removing(const Scenario::EventModel& e, EventComponent& c)
{
  return ScenarioExecImpl::removing_event(*this, e, c);
}

std::function<void()>
ScenarioComponentBase::removing(const Scenario::StateModel& e, StateComponent& c)
{
  return ScenarioExecImpl::removing_state(*this, e, c);
}

template <>
IntervalComponent*
ScenarioComponentBase::make<IntervalComponent, Scenario::IntervalModel>(
    Scenario::IntervalModel& cst)
{
  return ScenarioExecImpl::make_interval(*this, cst);
}

template <>
StateComponent* ScenarioComponentBase::make<StateComponent, Scenario::StateModel>(
    Scenario::StateModel& st)
{
  return ScenarioExecImpl::make_state(*this, st);
}

template <>
EventComponent* ScenarioComponentBase::make<EventComponent, Scenario::EventModel>(
    Scenario::EventModel& ev)
{
  return ScenarioExecImpl::make_event(*this, ev, [this, &ev](auto ossia_ev) {
    connect(
        &ev, &Scenario::EventModel::timeSyncChanged, this,
        [this, ossia_ev](
            const Id<Scenario::TimeSyncModel>& old_ts_id,
            const Id<Scenario::TimeSyncModel>& new_ts_id) {
      auto old_ts = m_ossia_timesyncs.at(old_ts_id);
      auto new_ts = m_ossia_timesyncs.at(new_ts_id);
      SCORE_ASSERT(old_ts && new_ts);
      SCORE_ASSERT(old_ts->OSSIATimeSync() && new_ts->OSSIATimeSync());
      m_ctx.executionQueue.enqueue(
          [old_t = old_ts->OSSIATimeSync(), new_t = new_ts->OSSIATimeSync(), ossia_ev] {
        old_t->remove(ossia_ev);
        new_t->insert(new_t->get_time_events().end(), ossia_ev);
        ossia_ev->set_time_sync(*new_t);
      });
        });
  });
}

template <>
TimeSyncComponent*
ScenarioComponentBase::make<TimeSyncComponent, Scenario::TimeSyncModel>(
    Scenario::TimeSyncModel& tn)
{
  return ScenarioExecImpl::make_timesync(*this, tn);
}

void ScenarioComponentBase::startIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::start_interval(*this, id);
}

void ScenarioComponentBase::disableIntervalExecution(
    const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::disable_interval(*this, id);
}

void ScenarioComponentBase::stopIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::stop_interval(*this, id);
}

void ScenarioComponentBase::eventCallback(
    std::weak_ptr<ScenarioComponentBase> self, std::shared_ptr<EventComponent> ev,
    ossia::time_event::status newStatus)
{
  ScenarioExecImpl::event_callback(*this, self, ev, newStatus, [this, newStatus](auto& score_state) {
    if(auto& c = score_state.previousInterval())
      m_properties.intervals[*c].status
          = static_cast<Scenario::ExecutionStatus>(newStatus);
  });
}
}
