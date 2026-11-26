#pragma once
#include <ossia/dataflow/dataflow_fwd.hpp>
#include <ossia/dataflow/exec_state_facade.hpp>

#include <score_lib_process_export.h>

#include <memory>
#include <vector>

namespace ossia
{
class graph_interface;
class graph_node;
struct execution_state;
struct inlet;
namespace net
{
class parameter_base;
class node_base;
}
}
namespace Process
{
class Port;
}
namespace Execution
{
struct Context;
struct ExecutionCommandQueue;
struct GCCommandQueue;

/**
 * A node that watches a port of a running process, linked into the execution
 * graph: it gets the same edges as the port (an outlet's, or every cable of
 * an inlet, and the inlet's address), and is ticked by the TapTicker. Used by
 * the hover probes (PortTap) and the recorders.
 *
 * Nothing changes in the document, and the port does what it does without a
 * tap.
 */
class SCORE_LIB_PROCESS_EXPORT TapLink
{
public:
  //! Links `node` (one inlet, of the port's type) to `port` in the running
  //! execution of `ctx`; linked() is false when the port does not run.
  TapLink(const Process::Port& port, const Execution::Context& ctx,
          std::shared_ptr<ossia::graph_node> node);
  //! Links `node` (one value inlet) to an address of a device: the node gets
  //! what the address receives, as an inlet bound to it would.
  TapLink(ossia::net::parameter_base& address, const Execution::Context& ctx,
          std::shared_ptr<ossia::graph_node> node);
  //! Runs `node` every tick without linking it to anything: a node that only
  //! reads the time of another (a clock probe)
  TapLink(const Execution::Context& ctx, std::shared_ptr<ossia::graph_node> node);
  //! What a device gives as a whole, as an inlet bound to its root gets it: the
  //! messages of a MIDI device
  TapLink(ossia::net::node_base& device_node, const Execution::Context& ctx,
          std::shared_ptr<ossia::graph_node> node);
  //! Unlinks the node, on the execution thread; it is freed on the GUI thread.
  ~TapLink();
  TapLink(const TapLink&) = delete;
  TapLink& operator=(const TapLink&) = delete;

  bool linked() const noexcept { return bool(m_node); }
  //! The inlet of the port when watching an inlet: what its address writes
  ossia::inlet* observedInlet() const noexcept { return m_observed; }

private:
  void link(const Execution::Context& ctx, std::shared_ptr<ossia::graph_node> node,
            ossia::net::parameter_base* address,
            ossia::net::node_base* node_address = nullptr);

  std::weak_ptr<ExecutionCommandQueue> m_queue;
  std::weak_ptr<GCCommandQueue> m_gc;
  std::weak_ptr<ossia::graph_interface> m_graph;
  std::weak_ptr<ossia::execution_state> m_state;
  std::shared_ptr<ossia::graph_node> m_node;
  std::vector<ossia::edge_ptr> m_edges;
  ossia::inlet* m_observed{};
};
}
