#pragma once
#include <Process/Dataflow/PortType.hpp>
#include <Process/ExecutionAction.hpp>
#include <Process/Monitoring/EventRing.hpp>
#include <Process/Monitoring/Monitor.hpp>
#include <Process/Monitoring/SampleRing.hpp>

#include <ossia/dataflow/dataflow_fwd.hpp>
#include <ossia/detail/lockfree_queue.hpp>
#include <ossia/network/dataspace/dataspace.hpp>
#include <ossia/network/value/value.hpp>

#include <score_lib_process_export.h>

#include <atomic>
#include <memory>
#include <vector>

namespace Process
{
class Port;
}
namespace Execution
{
struct Context;
struct ExecutionCommandQueue;
struct GCCommandQueue;

//! What a tap saw pass through a port, for a probe to show.
struct SCORE_LIB_PROCESS_EXPORT TapMonitor final : Process::Monitor
{
  explicit TapMonitor(Process::PortType t);
  ~TapMonitor() override;

  const Process::PortType type;

  //! Audio
  Process::SampleRing samples{1 << 15};

  //! MIDI, as MIDI 1 bytes
  struct Midi
  {
    uint8_t bytes[3]{};
    uint8_t size{};
  };
  Process::EventRing<Midi, 512> midi;

  //! Values. Full: newer values are dropped until the view catches up.
  ossia::spsc_queue<ossia::value> values{128};
  //! What the values are in: what the port declares, else its address.
  ossia::unit_t unit;
};

/**
 * Watches a port of a running process: what an outlet produces, or what an
 * inlet receives, its cables and its address merged as the process gets
 * them. While it exists, a node of the execution graph gets the same edges
 * as the port and copies what arrives into the monitor; it goes away with
 * the tap. Nothing is changed in the document, and the port does what it
 * does without a tap: an outlet whose only edge is the tap's still writes
 * to its address.
 *
 * A tap belongs to the execution it was made in: once that execution is
 * stopped or reloaded, monitor() is null and the tap only waits to be
 * destroyed.
 */
class SCORE_LIB_PROCESS_EXPORT PortTap
{
public:
  PortTap(const Process::Port& port, const Execution::Context& ctx);
  ~PortTap();
  PortTap(const PortTap&) = delete;
  PortTap& operator=(const PortTap&) = delete;

  //! Null when the port does not run, carries what a tap cannot copy, or
  //! the execution the tap was made in is over.
  TapMonitor* monitor() const noexcept;

private:
  // Not the context: it ends with playback, which the tap may outlive.
  std::weak_ptr<const Execution::Context> m_context;
  std::weak_ptr<ExecutionCommandQueue> m_queue;
  std::weak_ptr<GCCommandQueue> m_gc;
  std::weak_ptr<ossia::graph_interface> m_graph;
  std::weak_ptr<ossia::execution_state> m_state;
  std::shared_ptr<TapMonitor> m_monitor;
  std::shared_ptr<ossia::graph_node> m_node;
  std::vector<ossia::edge_ptr> m_edges;
};

/**
 * Taps are driven by no time process: this gives them a token at the start
 * of each tick, so that the graph runs them after their sources.
 *
 * The list of nodes is built on the GUI thread and published whole; the
 * execution thread only reads it. A replaced list is freed once the
 * execution thread cannot be reading it anymore. A node that is not in the
 * running graph keeps the one token it was given.
 */
class SCORE_LIB_PROCESS_EXPORT TapTicker final : public ExecutionAction
{
  SCORE_CONCRETE("0b7a5c1e-6f2d-4c9b-9a3e-5d8e2f71c4a6")
public:
  TapTicker();
  ~TapTicker() override;
  static TapTicker* instance() noexcept;

  void startTick(const ossia::audio_tick_state& st) override;

  //! GUI thread
  void add(std::shared_ptr<ossia::graph_node> node);
  void remove(const ossia::graph_node* node);

private:
  using node_list = std::vector<std::shared_ptr<ossia::graph_node>>;
  void publish();
  void collect();

  std::atomic<const node_list*> m_current{};
  //! Odd while startTick reads m_current
  std::atomic<uint64_t> m_ticks{};

  // GUI thread
  node_list m_nodes;
  std::unique_ptr<const node_list> m_published;
  struct retired
  {
    std::unique_ptr<const node_list> list;
    uint64_t ticks{};
  };
  std::vector<retired> m_retired;
};
}
