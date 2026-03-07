#pragma once
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/ExecutionTransaction.hpp>
#include <Process/Process.hpp>

#include <Curve/CurveConversion.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/Tempo/TempoProcess.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/curve/curve.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <ossia/editor/scenario/time_process.hpp>

namespace Execution
{

inline std::pair<std::optional<ossia::tempo_curve>, Scenario::TempoProcess*>
tempoCurve(const Scenario::IntervalModel& itv, const Execution::Context& ctx)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  // TODO
  if(auto proc = itv.tempoCurve())
  {
    auto& curve = proc->curve();
    // TODO recompute whenever tempo changes
    const auto defaultdur = itv.duration.defaultDuration().msec();
    auto scale_x = [&ctx, defaultdur](double val) -> int64_t {
      return ctx.time(TimeVal::fromMsecs(val * defaultdur)).impl;
    };
    auto scale_y = [=](double val) -> double {
      using namespace Scenario;
      return val * (TempoProcess::max - TempoProcess::min) + TempoProcess::min;
    };

    ossia::tempo_curve t;

    auto segt_data = curve.sortedSegments();
    if(segt_data.size() != 0)
    {
      t = std::move(*Engine::score_to_ossia::curve<int64_t, double>(
          scale_x, scale_y, segt_data, {}));
    }

    return {std::optional<ossia::tempo_curve>{std::move(t)}, proc};
  }
  else
  {
    return {};
  }
}
inline ossia::time_signature_map
timeSignatureMap(const Scenario::IntervalModel& itv, const Execution::Context& ctx)
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  ossia::time_signature_map ret;
  for(const auto& [time, sig] : itv.timeSignatureMap())
  {
    ret[ctx.time(time)] = sig;
  }
  return ret;
}

inline auto propagatedOutlets(const Process::Outlets& outlets) noexcept
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
  ossia::pod_vector<std::size_t> propagated_outlets;
  for(std::size_t i = 0; i < outlets.size(); i++)
  {
    if(outlets[i]->propagate())
      propagated_outlets.push_back(i);
  }
  return propagated_outlets;
}

inline void connectPropagated(
    const ossia::node_ptr& process_node, const ossia::node_ptr& interval_node,
    const ossia::node_ptr& gfx_fw_node,
    ossia::graph_interface& g,
    const ossia::pod_vector<std::size_t>& propagated_outlets) noexcept
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
  const auto& outs = process_node->root_outputs();
  for(std::size_t propagated : propagated_outlets)
  {
    if(propagated >= outs.size())
      continue;

    switch(outs[propagated]->which())
    {
      case ossia::audio_port::which:
      {
        auto cable = g.allocate_edge(
            ossia::immediate_glutton_connection{}, outs[propagated],
            interval_node->root_inputs()[0], process_node, interval_node);
        g.connect(cable);
        break;
      }
      case ossia::texture_port::which:
      {
        if(gfx_fw_node && !gfx_fw_node->root_inputs().empty())
        {
          auto cable = g.allocate_edge(
              ossia::immediate_glutton_connection{}, outs[propagated],
              gfx_fw_node->root_inputs()[0], process_node, gfx_fw_node);
          g.connect(cable);
        }
        break;
      }
      default:
        break;
    }
  }
}

inline void updatePropagated(
    const ossia::node_ptr& process_node, const ossia::node_ptr& interval_node,
    const ossia::node_ptr& gfx_fw_node,
    ossia::graph_interface& g, std::size_t port_idx, bool is_propagated) noexcept
{
  OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
  const auto& outs = process_node->root_outputs();

  if(port_idx >= outs.size())
    return;

  const ossia::outlet& outlet = *outs[port_idx];

  // Determine the target node based on port type
  ossia::node_ptr target_node;
  if(outlet.which() == ossia::audio_port::which)
    target_node = interval_node;
  else if(outlet.which() == ossia::texture_port::which)
    target_node = gfx_fw_node;
  else
    return;

  if(!target_node || target_node->root_inputs().empty())
    return;

  // Remove cables if depropagated, add cables if repropagated
  if(is_propagated)
  {
    for(const ossia::graph_edge* edge : outlet.targets)
    {
      if(edge->in_node == target_node)
        return;
    }

    auto cable = g.allocate_edge(
        ossia::immediate_glutton_connection{}, outs[port_idx],
        target_node->root_inputs()[0], process_node, target_node);
    g.connect(cable);
  }
  else
  {
    for(ossia::graph_edge* edge : outlet.targets)
    {
      if(edge->in_node == target_node)
      {
        g.disconnect(edge);
        return;
      }
    }
  }
}

struct AddProcess
{
  // Note : this looks a bit wtf, it is to handle RawPtrExecution
  std::shared_ptr<ossia::time_interval> cst;
  ossia::time_interval* cst_ptr{};
  std::weak_ptr<ossia::time_process> oproc_weak;
  std::weak_ptr<ossia::graph_interface> g_weak;
  std::weak_ptr<ossia::graph_node> gfx_forward_weak;
  ossia::pod_vector<std::size_t> propagated_outlets;

  void operator()() const noexcept
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    auto oproc = oproc_weak.lock();
    if(!oproc)
      return;

    auto g = g_weak.lock();
    if(!g)
      return;

    cst_ptr->add_time_process(oproc);
    if(!oproc->node)
      return;

    connectPropagated(oproc->node, cst_ptr->node, gfx_forward_weak.lock(), *g, propagated_outlets);
  }
};

struct RecomputePropagate
{
  const Execution::Context& system;
  Process::ProcessModel& proc;
  std::weak_ptr<ossia::graph_node> cst_node_weak;
  std::weak_ptr<ossia::graph_node> gfx_forward_weak;
  std::weak_ptr<ossia::time_process> oproc_weak;
  std::weak_ptr<ossia::graph_interface> g_weak;
  Process::Outlet* outlet{};

  void operator()(bool propagate) const noexcept
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);

    // TODO find a better way !
    auto port_index
        = std::distance(proc.outlets().begin(), ossia::find(proc.outlets(), outlet));

    system.executionQueue.enqueue(
        [cst_node_weak = this->cst_node_weak, gfx_fw_weak = this->gfx_forward_weak,
         g_weak = this->g_weak, oproc_weak = this->oproc_weak, port_index, propagate] {
      const auto g = g_weak.lock();
      if(!g)
        return;

      const auto cst_node = cst_node_weak.lock();
      if(!cst_node)
        return;

      const auto oproc = oproc_weak.lock();
      if(!oproc)
        return;

      const auto& proc_node = oproc->node;
      if(!oproc->node)
        return;

      updatePropagated(proc_node, cst_node, gfx_fw_weak.lock(), *g, port_index, propagate);
    });
  }
};

template <typename T>
struct ReconnectOutlets
{
  T& component;
  std::weak_ptr<ossia::graph_node> fw_node;
  std::weak_ptr<ossia::graph_node> gfx_fw_node;

  Process::ProcessModel& proc;
  std::weak_ptr<ossia::time_process> oproc_weak;

  std::weak_ptr<ossia::graph_interface> g_weak;

  void operator()() const noexcept
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);
    for(Process::Outlet* outlet : proc.outlets())
    {
      if(outlet->propagate() || qobject_cast<Process::AudioOutlet*>(outlet)
         || outlet->type() == Process::PortType::Texture)
      {
        QObject::disconnect(
            outlet, &Process::Outlet::propagateChanged, &component, nullptr);
        QObject::connect(
            outlet, &Process::Outlet::propagateChanged, &component,
            RecomputePropagate{
                component.system(), proc, fw_node, gfx_fw_node, oproc_weak, g_weak, outlet});
      }
    }
  }
};

struct HandleNodeChange
{
  std::weak_ptr<ossia::graph_node> cst_node_weak;
  std::weak_ptr<ossia::graph_node> gfx_forward_weak;
  std::weak_ptr<ossia::time_process> oproc_weak;
  std::weak_ptr<ossia::graph_interface> g_weak;
  Process::ProcessModel& proc;

  void operator()(
      const ossia::node_ptr& old_node, const ossia::node_ptr& new_node,
      Execution::Transaction* commands) const noexcept
  {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Ui);

    commands->push_back([cst_node_weak = this->cst_node_weak,
                         gfx_fw_weak = this->gfx_forward_weak,
                         g_weak = this->g_weak,
                         propagated = propagatedOutlets(proc.outlets()), old_node,
                         new_node] {
      OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
      auto cst_node = cst_node_weak.lock();
      if(!cst_node)
        return;
      auto g = g_weak.lock();
      if(!g)
        return;
      auto gfx_fw = gfx_fw_weak.lock();

      // Remove propagate edges from the old node
      if(old_node)
      {
        ossia::graph_node& n = *old_node;
        for(auto& outlet : n.root_outputs())
        {
          auto targets = outlet->targets;
          for(auto e : targets)
          {
            if(e->in_node.get() == cst_node.get()
               || (gfx_fw && e->in_node.get() == gfx_fw.get()))
            {
              g->disconnect(e);
            }
          }
        }
      }

      // Add edges to the new node
      if(new_node)
      {
        connectPropagated(new_node, cst_node, gfx_fw, *g, propagated);
      }
    });
  }
};

}
