#include "TapLink.hpp"

#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Execution/PortTap.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Process/ExecutionTransaction.hpp>

#include <score/model/path/PathSerialization.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph/graph_interface.hpp>
#include <ossia/dataflow/graph_edge.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

namespace Execution
{
void writeInPlaceOfOutlets(ossia::inlet& tap, ossia::exec_state_facade e) noexcept
{
  for(ossia::graph_edge* edge : tap.sources)
    if(edge->out && edge->out_node && edge->out_node->executed()
       && edge->out->targets.size() == 1)
      edge->out->write(*e.impl);
}

TapLink::TapLink(
    const Process::Port& port, const Execution::Context& ctx,
    std::shared_ptr<ossia::graph_node> node)
    : m_queue{ctx.weakExecutionQueue()}
    , m_gc{ctx.weakGCQueue()}
    , m_graph{ctx.execGraph}
    , m_state{ctx.execState}
{
  auto* ticker = TapTicker::instance();
  if(!node || !ticker || ctx.retired || !ctx.execGraph)
    return;

  auto& setup = ctx.setup;
  auto* tap_inlet = node->root_inputs().front();

  auto connect_from = [&](Process::Outlet* outlet) {
    auto it = setup.outlets.find(outlet);
    if(it == setup.outlets.end() || !it->second.first || !it->second.second)
      return;
    m_edges.push_back(ctx.execGraph->allocate_edge(
        ossia::immediate_glutton_connection{}, it->second.second,
        ossia::inlet_ptr{tap_inlet}, it->second.first, node));
  };

  if(auto* outlet = qobject_cast<const Process::Outlet*>(&port))
  {
    connect_from(const_cast<Process::Outlet*>(outlet));
  }
  else if(auto* inlet = qobject_cast<const Process::Inlet*>(&port))
  {
    // What the process receives: every cable, and its address below
    for(const auto& path : inlet->cables())
      if(auto* cable = path.try_find(ctx.doc))
        if(auto* src = cable->source().try_find(ctx.doc))
          connect_from(src);
    if(auto it = setup.inlets.find(const_cast<Process::Inlet*>(inlet));
       it != setup.inlets.end())
      m_observed = it->second.second;
  }

  if(m_edges.empty() && !m_observed)
    return;
  link(ctx, std::move(node), nullptr);
}

TapLink::TapLink(
    ossia::net::parameter_base& address, const Execution::Context& ctx,
    std::shared_ptr<ossia::graph_node> node)
    : m_queue{ctx.weakExecutionQueue()}
    , m_gc{ctx.weakGCQueue()}
    , m_graph{ctx.execGraph}
    , m_state{ctx.execState}
{
  if(!node || !TapTicker::instance() || ctx.retired || !ctx.execGraph)
    return;
  link(ctx, std::move(node), &address);
}

TapLink::TapLink(
    ossia::net::node_base& device_node, const Execution::Context& ctx,
    std::shared_ptr<ossia::graph_node> node)
    : m_queue{ctx.weakExecutionQueue()}
    , m_gc{ctx.weakGCQueue()}
    , m_graph{ctx.execGraph}
    , m_state{ctx.execState}
{
  if(!node || !TapTicker::instance() || ctx.retired || !ctx.execGraph)
    return;
  link(ctx, std::move(node), nullptr, &device_node);
}

TapLink::TapLink(const Execution::Context& ctx, std::shared_ptr<ossia::graph_node> node)
    : m_queue{ctx.weakExecutionQueue()}
    , m_gc{ctx.weakGCQueue()}
    , m_graph{ctx.execGraph}
    , m_state{ctx.execState}
{
  if(!node || !TapTicker::instance() || ctx.retired || !ctx.execGraph)
    return;
  link(ctx, std::move(node), nullptr);
}

void TapLink::link(
    const Execution::Context& ctx, std::shared_ptr<ossia::graph_node> node,
    ossia::net::parameter_base* address, ossia::net::node_base* node_address)
{
  m_node = std::move(node);
  ctx.executionQueue.enqueue([g = ctx.execGraph, ws = std::weak_ptr{ctx.execState},
                              node = m_node, edges = m_edges, observed = m_observed,
                              address, node_address] {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    g->add_node(node);
    for(auto& e : edges)
      g->connect(e);
    auto& tap = *node->root_inputs().front();
    if(observed)
      tap.address = observed->address;
    else if(address)
      tap.address = address;
    else if(node_address)
      tap.address = node_address;
    if(observed || address || node_address)
      if(auto s = ws.lock())
        s->register_port(tap);
    g->mark_dirty();
  });
  TapTicker::instance()->add(m_node);
}

TapLink::~TapLink()
{
  if(!m_node)
    return;
  if(auto* ticker = TapTicker::instance())
    ticker->remove(m_node.get());

  // Gone with its execution otherwise, and the graph with it
  auto q = m_queue.lock();
  if(!q)
    return;
  q->enqueue([wg = m_graph, ws = m_state, gcq = m_gc, node = std::move(m_node),
              edges = std::move(m_edges)]() mutable {
    OSSIA_ENSURE_CURRENT_THREAD_KIND(ossia::thread_type::Audio);
    if(auto s = ws.lock())
      s->unregister_port(*node->root_inputs().front());
    if(auto g = wg.lock())
    {
      for(auto& e : edges)
        g->disconnect(e);
      g->remove_node(node);
      g->mark_dirty();
    }
    // Freed on the GUI thread
    if(auto q = gcq.lock())
      q->enqueue(gc(std::move(node), std::move(edges)));
  });
}
}
