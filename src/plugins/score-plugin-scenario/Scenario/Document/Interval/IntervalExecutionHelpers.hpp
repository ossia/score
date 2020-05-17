#pragma once
#include <Curve/CurveConversion.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/Process.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Tempo/TempoProcess.hpp>

#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/curve/curve.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

namespace Execution
{

inline std::pair<optional<ossia::tempo_curve>, Scenario::TempoProcess*>
tempoCurve(const Scenario::IntervalModel& itv, const Execution::Context& ctx)
{
  // TODO
  if (auto proc = itv.tempoCurve())
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
    if (segt_data.size() != 0)
    {
      t = std::move(
          *Engine::score_to_ossia::curve<int64_t, double>(scale_x, scale_y, segt_data, {}));
    }

    return {optional<ossia::tempo_curve>{std::move(t)}, proc};
  }
  else
  {
    return {};
  }
}
inline optional<ossia::time_signature_map>
timeSignatureMap(const Scenario::IntervalModel& itv, const Execution::Context& ctx)
{
  if (itv.hasTimeSignature())
  {
    ossia::time_signature_map ret;
    for (const auto& [time, sig] : itv.timeSignatureMap())
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
    if (auto o = qobject_cast<Process::AudioOutlet*>(outlets[i]))
      if (o->propagate())
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
  const auto& outs = process_node->root_outputs();
  for (std::size_t propagated : propagated_outlets)
  {
    if (propagated >= outs.size())
      continue;

    if (outs[propagated]->which() == ossia::audio_port::which)
    {
      auto cable = ossia::make_edge(
          ossia::immediate_glutton_connection{},
          outs[propagated],
          interval_node->root_inputs()[0],
          process_node,
          interval_node);
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
  const auto& outs = process_node->root_outputs();

  if (port_idx >= outs.size())
    return;

  const ossia::outlet& outlet = *outs[port_idx];

  if (!outlet.target<ossia::audio_port>())
    return;

  // Remove cables if depropagated, add cables if repropagated
  if (is_propagated)
  {
    for (const ossia::graph_edge* edge : outlet.targets)
    {
      if (edge->in_node == interval_node)
        return;
    }

    auto cable = ossia::make_edge(
        ossia::immediate_glutton_connection{},
        outs[port_idx],
        interval_node->root_inputs()[0],
        process_node,
        interval_node);
    g.connect(cable);
  }
  else
  {
    for (ossia::graph_edge* edge : outlet.targets)
    {
      if (edge->in_node == interval_node)
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
  ossia::pod_vector<std::size_t> propagated_outlets;

  void operator()() const noexcept
  {
    auto oproc = oproc_weak.lock();
    if (!oproc)
      return;

    auto g = g_weak.lock();
    if (!g)
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

  void operator()(bool propagate) const noexcept
  {

    // TODO find a better way !
    auto port_index = std::distance(proc.outlets().begin(), ossia::find(proc.outlets(), outlet));

    system.executionQueue.enqueue([cst_node_weak = this->cst_node_weak,
                                   g_weak = this->g_weak,
                                   oproc_weak = this->oproc_weak,
                                   port_index,
                                   propagate] {
      const auto g = g_weak.lock();
      if (!g)
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

template <typename T>
struct ReconnectOutlets
{
  T& component;
  std::weak_ptr<ossia::graph_node> fw_node;

  Process::ProcessModel& proc;
  std::weak_ptr<ossia::time_process> oproc_weak;

  std::weak_ptr<ossia::graph_interface> g_weak;

  void operator()() const noexcept
  {
    for (Process::Outlet* outlet : proc.outlets())
    {
      if (auto o = qobject_cast<Process::AudioOutlet*>(outlet))
      {
        QObject::disconnect(o, &Process::AudioOutlet::propagateChanged, &component, nullptr);
        QObject::connect(
            o,
            &Process::AudioOutlet::propagateChanged,
            &component,
            RecomputePropagate{component.system(), proc, fw_node, oproc_weak, g_weak, outlet});
      }
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
      Execution::Transaction& commands) const noexcept
  {

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
        for (auto& outlet : n.root_outputs())
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
