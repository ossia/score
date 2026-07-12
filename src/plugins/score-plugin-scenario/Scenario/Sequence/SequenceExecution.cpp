// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Process/ExecutionContext.hpp>

#include <Scenario/Document/Event/EventExecution.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalExecution.hpp>
#include <Scenario/Document/State/StateExecution.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncExecution.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/ScenarioExecImpl.hpp>
#include <Scenario/Sequence/SequenceExecution.hpp>
#include <Scenario/Sequence/SequenceModel.hpp>

#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>

#include <Automation/AutomationModel.hpp>

#include <Color/GradientModel.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/nodes/forward_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/editor/scenario/scenario.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Execution::SequenceComponentBase)

namespace
{
// The sequence's process node: forwards its children's audio like a scenario,
// and additionally forwards one value stream per sequence parameter — the
// value inlets are fed by edges from the section automations, the outlets are
// registered against the SequenceModel's per-parameter document ports.
class sequence_node final : public ossia::nodes::forward_node
{
public:
  explicit sequence_node(std::size_t n_params)
  {
    m_pins.reserve(n_params);
    for(std::size_t i = 0; i < n_params; i++)
    {
      auto in = std::make_unique<ossia::value_inlet>();
      auto out = std::make_unique<ossia::value_outlet>();
      m_inlets.push_back(in.get());
      m_outlets.push_back(out.get());
      m_pins.emplace_back(std::move(in), std::move(out));
    }
  }

  [[nodiscard]] std::string label() const noexcept override { return "sequence_node"; }

  void
  run(const ossia::token_request& t, ossia::exec_state_facade f) noexcept override
  {
    ossia::nodes::forward_node::run(t, f);
    for(auto& [in, out] : m_pins)
    {
      ossia::value_port& ip = **in;
      ossia::value_port& op = **out;
      for(const auto& tv : ip.get_data())
        op.write_value(tv.value, tv.timestamp);
    }
  }

private:
  std::vector<
      std::pair<std::unique_ptr<ossia::value_inlet>, std::unique_ptr<ossia::value_outlet>>>
      m_pins;
};
}

namespace Execution
{

SequenceComponentBase::SequenceComponentBase(
    Sequence::SequenceModel& element, const Context& ctx, QObject* parent)
    : ProcessComponent_T<Sequence::SequenceModel, ossia::scenario>{
        element, ctx, "SequenceComponent", nullptr}
    , m_ctx{ctx}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  this->setObjectName("OSSIASequenceElement");

  m_ossia_process = std::make_shared<ossia::scenario>();
  // Substitute the scenario's default forward_node with our variant carrying
  // one value in/out pair per parameter.
  auto snode = std::make_shared<sequence_node>(element.parameterNamespace().size());
  m_ossia_process->node = snode;
  node = snode;
  snode->audio_in.sources.reserve(element.intervals.size() * 2);

  connect(
      this, &SequenceComponentBase::sig_eventCallback, this,
      &SequenceComponentBase::eventCallback, Qt::QueuedConnection);
}

SequenceComponentBase::~SequenceComponentBase()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
}

SequenceComponent::SequenceComponent(
    Sequence::SequenceModel& proc, const Context& ctx, QObject* parent)
    : SequenceComponentHierarchy{score::lazy_init_t{}, proc, ctx, parent}
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
}

void SequenceComponent::lazy_init()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  SequenceComponentHierarchy::init();
  setupParamForwarding();
}

void SequenceComponentBase::setupParamForwarding()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  const auto& ns = process().parameterNamespace();
  if(ns.isEmpty())
    return;

  // Node inputs: [0] = audio_in, [1 + i] = value inlet for parameter i
  for(auto& [iid, icomp] : m_ossia_intervals)
  {
    if(!icomp)
      continue;
    for(auto& [pid, pcomp] : icomp->processes())
    {
      if(!pcomp || !pcomp->node)
        continue;

      State::AddressAccessor addr;
      if(auto* a = qobject_cast<Automation::ProcessModel*>(&pcomp->process()))
        addr = a->address();
      else if(auto* g = qobject_cast<Gradient::ProcessModel*>(&pcomp->process()))
        addr = g->address();
      else
        continue;

      const int idx = ns.indexOf(addr);
      if(idx < 0)
        continue;

      const auto& src = pcomp->node;
      if(src->root_outputs().empty()
         || std::size_t(1 + idx) >= node->root_inputs().size())
        continue;

      auto edge = m_ctx.execGraph->allocate_edge(
          ossia::immediate_glutton_connection{}, src->root_outputs()[0],
          node->root_inputs()[1 + idx], src, node);

      std::weak_ptr<ossia::graph_interface> wg = m_ctx.execGraph;
      m_ctx.executionQueue.enqueue([wg, edge]() mutable {
        if(auto g = wg.lock())
          g->connect(std::move(edge));
      });
    }
  }
}

void SequenceComponent::cleanup()
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  clear();
  ProcessComponent::cleanup();
}

void SequenceComponentBase::stop()
{
  ScenarioExecImpl::stop_body(*this);
  ProcessComponent::stop();
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::IntervalModel& e, IntervalComponent& c)
{
  return ScenarioExecImpl::removing_interval(*this, e, c);
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::TimeSyncModel& e, TimeSyncComponent& c)
{
  return ScenarioExecImpl::removing_timesync(*this, e, c);
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::EventModel& e, EventComponent& c)
{
  return ScenarioExecImpl::removing_event(*this, e, c);
}

std::function<void()>
SequenceComponentBase::removing(const Scenario::StateModel& e, StateComponent& c)
{
  return ScenarioExecImpl::removing_state(*this, e, c);
}

template <>
IntervalComponent*
SequenceComponentBase::make<IntervalComponent, Scenario::IntervalModel>(
    Scenario::IntervalModel& cst)
{
  return ScenarioExecImpl::make_interval(*this, cst);
}

template <>
StateComponent* SequenceComponentBase::make<StateComponent, Scenario::StateModel>(
    Scenario::StateModel& st)
{
  return ScenarioExecImpl::make_state(*this, st);
}

template <>
EventComponent* SequenceComponentBase::make<EventComponent, Scenario::EventModel>(
    Scenario::EventModel& ev)
{
  return ScenarioExecImpl::make_event(*this, ev, [](auto&) {});
}

template <>
TimeSyncComponent*
SequenceComponentBase::make<TimeSyncComponent, Scenario::TimeSyncModel>(
    Scenario::TimeSyncModel& tn)
{
  return ScenarioExecImpl::make_timesync(*this, tn);
}

void SequenceComponentBase::startIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::start_interval(*this, id);
}

void SequenceComponentBase::disableIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::disable_interval(*this, id);
}

void SequenceComponentBase::stopIntervalExecution(const Id<Scenario::IntervalModel>& id)
{
  ScenarioExecImpl::stop_interval(*this, id);
}

void SequenceComponentBase::eventCallback(
    std::weak_ptr<SequenceComponentBase> self, std::shared_ptr<EventComponent> ev,
    ossia::time_event::status newStatus)
{
  ScenarioExecImpl::event_callback(*this, self, ev, newStatus, [](auto&) {});
}

} // namespace Execution
