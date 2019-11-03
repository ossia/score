#pragma once
#include <Process/Process.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/editor/scenario/time_interval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
namespace Execution
{


inline optional<ossia::tempo_curve> tempoCurve(
    const Scenario::IntervalModel& itv,
    const Execution::Context& ctx)
{
  // TODO
  if(itv.hasTempo())
  {
    ossia::tempo_curve t;
    t.set_x0(0);
    t.set_y0(120.);
    return optional<ossia::tempo_curve>{std::move(t)};
  }
  else
  {
    return ossia::none;
  }
}
inline optional<ossia::time_signature_map> timeSignatureMap(
    const Scenario::IntervalModel& itv,
    const Execution::Context& ctx)
{
  if(itv.hasTimeSignature())
  {
    ossia::time_signature_map ret;
    for(const auto& [time, sig] : itv.timeSignatureMap())
    {
      ret[ctx.time(time)] = sig;
    }
    return ret;
  }
  else
  {
    return ossia::none;
  }
}

inline auto propagatedOutlets(const Process::Outlets& outlets) noexcept
{
  ossia::pod_vector<std::size_t> propagated_outlets;
  for (std::size_t i = 0; i < outlets.size(); i++)
  {
    if (outlets[i]->propagate())
      propagated_outlets.push_back(i);
  }
  return propagated_outlets;
}

inline void connectPropagated(
      const ossia::node_ptr& process_node,
      const ossia::node_ptr& interval_node,
      ossia::graph_interface& g,
      const ossia::pod_vector<std::size_t>& propagated_outlets) noexcept
{
  const auto& outs = process_node->outputs();
  for (std::size_t propagated : propagated_outlets)
  {
    if(propagated >= outs.size())
      continue;

    const auto& outlet = outs[propagated]->data;
    if (outlet.target<ossia::audio_port>())
    {
      auto cable = ossia::make_edge(
            ossia::immediate_glutton_connection{},
            outs[propagated], interval_node->inputs()[0],
            process_node, interval_node);
      g.connect(cable);
    }
  }
}

inline void updatePropagated(
    const ossia::node_ptr& process_node,
    const ossia::node_ptr& interval_node,
    ossia::graph_interface& g,
    std::size_t port_idx,
    bool is_propagated) noexcept
{
  const auto& outs = process_node->outputs();

  if(port_idx >= outs.size())
    return;

  const ossia::outlet& outlet = *outs[port_idx];

  if (!outlet.data.target<ossia::audio_port>())
    return;

  // Remove cables if depropagated, add cables if repropagated
  if(is_propagated)
  {
    for(const ossia::graph_edge* edge : outlet.targets)
    {
      if(edge->in_node == interval_node)
        return;
    }

    auto cable = ossia::make_edge(
          ossia::immediate_glutton_connection{},
          outs[port_idx], interval_node->inputs()[0],
        process_node, interval_node);
    g.connect(cable);
  }
  else
  {
    for(ossia::graph_edge* edge : outlet.targets)
    {
      if(edge->in_node == interval_node)
      {
        g.disconnect(edge);
        return;
      }
    }
  }
}

struct AddProcess
{
  std::shared_ptr<ossia::time_interval> cst;
  ossia::time_interval* cst_ptr{};
  std::weak_ptr<ossia::time_process> oproc_weak;
  std::weak_ptr<ossia::graph_interface> g_weak;
  ossia::pod_vector<std::size_t> propagated_outlets;

  void operator()() const noexcept
  {
    auto oproc = oproc_weak.lock();
    if (!oproc)
      return;

    auto g = g_weak.lock();
    if(!g)
      return;

    cst_ptr->add_time_process(oproc);
    if (!oproc->node)
      return;

    connectPropagated(oproc->node, cst_ptr->node, *g, propagated_outlets);
  }
};

struct RecomputePropagate
{
  const Execution::Context& system;
  Process::ProcessModel& proc;
  std::weak_ptr<ossia::graph_node> cst_node_weak;
  std::weak_ptr<ossia::time_process> oproc_weak;
  std::weak_ptr<ossia::graph_interface> g_weak;
  Process::Outlet* outlet{};

  void operator() (bool propagate) const noexcept {

    // TODO find a better way !
    auto port_index = std::distance(proc.outlets().begin(), ossia::find(proc.outlets(), outlet));

    system.executionQueue.enqueue([cst_node_weak=this->cst_node_weak
            , g_weak=this->g_weak
            , oproc_weak = this->oproc_weak
            , port_index
            , propagate] {
      const auto g = g_weak.lock();
      if(!g)
        return;

      const auto cst_node = cst_node_weak.lock();
      if (!cst_node)
        return;

      const auto oproc = oproc_weak.lock();
      if (!oproc)
        return;

      const auto& proc_node = oproc->node;

      updatePropagated(proc_node, cst_node, *g, port_index, propagate);
    });
  }
};

template<typename T>
struct ReconnectOutlets
{
  T& component;
  Process::ProcessModel& proc;
  std::weak_ptr<ossia::time_process> oproc_weak;
  std::weak_ptr<ossia::graph_interface> g_weak;

  void operator()() const noexcept
  {
    for(Process::Outlet* outlet : proc.outlets())
    {
      QObject::disconnect(outlet, &Process::Outlet::propagateChanged, &component, nullptr);
      QObject::connect(outlet, &Process::Outlet::propagateChanged,
                       &component, RecomputePropagate{
                         component.system(),
                         proc,
                         component.OSSIAInterval()->node,
                         oproc_weak,
                         g_weak,
                         outlet});
    }
  }
};

struct HandleNodeChange
{
  std::weak_ptr<ossia::graph_node> cst_node_weak;
  std::weak_ptr<ossia::time_process> oproc_weak;
  std::weak_ptr<ossia::graph_interface> g_weak;
  Process::ProcessModel& proc;

  void operator()(
        const ossia::node_ptr& old_node,
        const ossia::node_ptr& new_node,
        std::vector<Execution::ExecutionCommand>& commands) const noexcept {

    commands.push_back([cst_node_weak = this->cst_node_weak,
                        g_weak = this->g_weak,
                        propagated = propagatedOutlets(proc.outlets()),
                        old_node,
                        new_node] {
      auto cst_node = cst_node_weak.lock();
      if (!cst_node)
        return;
      auto g = g_weak.lock();
      if (!g)
        return;

      // Remove edges from the old node
      if (old_node)
      {
        ossia::graph_node& n = *old_node;
        for (auto& outlet : n.outputs())
        {
          auto targets = outlet->targets;
          for (auto e : targets)
          {
            if (e->in_node.get() == cst_node.get())
            {
              g->disconnect(e);
            }
          }
        }
      }

      // Add edges to the new node
      if (new_node)
      {
        connectPropagated(new_node, cst_node, *g, propagated);
      }
    });
  }
};

}
