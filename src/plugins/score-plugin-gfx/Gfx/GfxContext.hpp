#pragma once
#include <Process/Dataflow/CableData.hpp>

#include <Gfx/AssetTable.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderClock.hpp>

#include <score/tools/Timers.hpp>

#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/buffer_pool.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/mutex.hpp>
#include <ossia/detail/small_flat_map.hpp>
#include <ossia/detail/variant.hpp>
#include <ossia/gfx/port_index.hpp>
#include <ossia/network/value/value.hpp>

#include <concurrentqueue.h>
#include <score_plugin_gfx_export.h>
class QTimer;
namespace score {
class HighResolutionTimer;
class Timers;
}
namespace score::gfx
{
struct Graph;
class OutputNode;
}
namespace score
{
struct DocumentContext;
}
namespace Gfx
{
using port_index = ossia::gfx::port_index;
struct EdgeSpec
{
  port_index first{};
  port_index second{};
  Process::CableType type{};

  // type takes part in identity: the exec thread republishes the full edge
  // set every tick and endTick dedups against prev_edges — comparisons that
  // ignore the cable type made an Immediate/Delayed flip invisible (never
  // published, so the graph kept the old type until an unrelated change).
  // A type flip now diffs as remove(old) + add(new) in the incremental path.
  bool operator==(const EdgeSpec& other) const noexcept
  {
    return first == other.first && second == other.second && type == other.type;
  }
  bool operator!=(const EdgeSpec& other) const noexcept { return !(*this == other); }
  bool operator<(const EdgeSpec& other) const noexcept
  {
    if(first != other.first)
      return first < other.first;
    if(second != other.second)
      return second < other.second;
    return type < other.type;
  }
};

class GfxExecutionAction;
class SCORE_PLUGIN_GFX_EXPORT GfxContext : public QObject
{
  friend class GfxExecutionAction;

public:
  using NodePtr = std::unique_ptr<score::gfx::Node>;

  explicit GfxContext(const score::DocumentContext& ctx);
  ~GfxContext();

  int32_t register_node(NodePtr node);
  int32_t register_preview_node(NodePtr node);
  void connect_preview_node(EdgeSpec e);
  void disconnect_preview_node(EdgeSpec e);
  void unregister_node(int32_t idx);
  void unregister_preview_node(int32_t idx);

  // Synchronously tear down an output node's render list and drop it from the
  // graph. Used by device-owned outputs (e.g. the offscreen BackgroundNode)
  // whose destructor runs during shutdown before the async node-command queue
  // is drained — without this the graph is left with a dangling output pointer
  // and a RenderList referencing an already-freed QRhi.
  void destroyOutput(score::gfx::OutputNode* node);

  void recompute_edges();
  void recompute_graph();
  void recompute_connections();
  void recomputeTimers();
  void recomputeGraphTopology();
  void incrementalEdgeUpdate(
      const ossia::flat_set<EdgeSpec>& old_edges,
      const ossia::flat_set<EdgeSpec>& cur_edges);

  void update_inputs();
  void updateGraph();

  /**
   * @brief Render exactly @p frames times, synchronously, and return.
   *
   * The normal path renders off wall-clock timers, so "how many frames have I
   * drawn" depends on how long the caller happened to wait and how fast the
   * machine is -- a harness that sleeps and then grabs gets a different frame
   * on a Raspberry Pi than on a workstation, and an animated shader gives a
   * different image every run.
   *
   * Stepping instead makes frame N mean the same thing everywhere, which is
   * what lets a rendered frame be compared against a stored reference at all.
   *
   * Each step also hands every process node a synthetic token of
   * `frame / stepRate()`, so TIME, TIMEDELTA, FRAMEINDEX and PROGRESS follow the
   * counter rather than whatever the transport last delivered. Without it an
   * animated shader keeps reading the execution clock and frame N is a
   * different picture every run.
   *
   * Still on the execution clock: whatever a node takes from the transport
   * itself rather than from its process UBO -- video decode position,
   * automation -- so a graph built on those is only as reproducible as that
   * clock is.
   */
  void renderFrames(int frames);

  //! Step used by renderFrames(), in frames per second.
  double stepRate() const noexcept { return m_stepRate; }
  void setStepRate(double fps) noexcept { m_stepRate = fps; }

  void send_message(score::gfx::Message&& msg) noexcept
  {
    tick_messages.enqueue(std::move(msg));
  }

  /**
   * @brief Session-wide content-hash decode cache.
   *
   * Shared across all RenderLists in this GfxContext. Loaders stage
   * decoded bytes here on their worker thread; downstream consumers
   * (texture upload, mesh VB/IB assembly) acquire by content hash,
   * avoiding re-decoding the same source asset across multiple outputs
   * or reloads. See Gfx/AssetTable.hpp.
   */
  AssetTable& assets() noexcept { return m_assets; }
  const AssetTable& assets() const noexcept { return m_assets; }

private:
  void run_commands();
  void add_preview_output(score::gfx::OutputNode& out);
  void remove_preview_output();
  void add_edge(EdgeSpec e);
  void remove_edge(EdgeSpec e);
  void remove_node(std::vector<std::unique_ptr<score::gfx::Node>>& nursery, int32_t id);

  void on_no_vsync_timer(score::HighResolutionTimer* self);
  void on_watchdog_timer(score::HighResolutionTimer* self);
  const score::DocumentContext& m_context;
  std::atomic_int32_t index{1};
  ossia::hash_map<int32_t, NodePtr> nodes;

  score::gfx::Graph* m_graph{};
  QThread m_thread;

  double m_stepRate{60.};
  int64_t m_stepFrame{};

  struct NodeCommand
  {
    enum
    {
      ADD_NODE,
      ADD_PREVIEW_NODE,
      REMOVE_NODE,
      REMOVE_PREVIEW_NODE,
      RELINK
    } cmd{};
    int32_t index{};
    std::unique_ptr<score::gfx::Node> node;
  };

  struct EdgeCommand
  {
    enum
    {
      CONNECT_PREVIEW_NODE,
      DISCONNECT_PREVIEW_NODE
    } cmd{};
    EdgeSpec edge;
  };

  using Command = ossia::variant<NodeCommand, EdgeCommand>;
  moodycamel::ConcurrentQueue<Command> tick_commands;
  moodycamel::ConcurrentQueue<score::gfx::Message> tick_messages;
  std::vector<std::pair<score::gfx::Message, int>> m_deferredMessages;

  std::mutex edges_lock;
  ossia::flat_set<EdgeSpec> new_edges TS_GUARDED_BY(edges_lock);
  ossia::flat_set<EdgeSpec> edges TS_GUARDED_BY(edges_lock);
  ossia::flat_set<EdgeSpec> preview_edges TS_GUARDED_BY(edges_lock);
  std::atomic_bool edges_changed{};
  bool m_fullRebuildThisFrame{};

  score::HighResolutionTimer* m_no_vsync_timer{};
  score::HighResolutionTimer* m_watchdog_timer{};
  // Rate == 0 with vsync off: zero-interval timer, renders back-to-back.
  QTimer* m_freewheel_timer{};

  // Per-output render clocks (the render-clock / genlock abstraction).
  //
  // These replace the old timer->set<OutputNode*> map: each TimerClock owns
  // one shared HighResolutionTimer at a given manualRenderingRate and the
  // coalesced set of outputs driven by it (clock #2, the default), while the
  // single DisplayVSyncClock wraps the swap-chain vsync callback (clock #1).
  // Behaviour is byte-identical to the previous inline timer bookkeeping.
  std::vector<std::unique_ptr<score::gfx::TimerClock>> m_renderClocks;
  std::unique_ptr<score::gfx::DisplayVSyncClock> m_vsyncClock;

  ossia::object_pool<std::vector<score::gfx::gfx_input>> m_buffers;

  AssetTable m_assets;

  score::Timers m_timers;
};

}
