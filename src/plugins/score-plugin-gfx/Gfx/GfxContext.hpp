#pragma once
#include <span>
#include <Process/Dataflow/CableData.hpp>

#include <Gfx/AssetTable.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderClock.hpp>

#include <score/tools/Timers.hpp>

#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/buffer_pool.hpp>
#include <ossia/detail/flat_map.hpp>
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
struct QRhiReadbackResult;
namespace score::gfx
{
struct PreviewTap;
struct PreviewHubNode;
struct BackgroundNode;
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
  // set every tick and endTick dedups against prev_edges, so a comparison that
  // ignored the cable type would make an Immediate/Delayed flip invisible. As
  // it stands a type flip diffs as remove(old) + add(new) in the incremental
  // path.
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
  int32_t register_preview_node(std::unique_ptr<score::gfx::OutputNode> node);
  void connect_preview_node(EdgeSpec e);
  void disconnect_preview_node(EdgeSpec e);
  void unregister_node(int32_t idx);
  void unregister_preview_node(int32_t idx);

  /**
   * @brief Previews what @p producer outputs, into @p readback, at most @p size.
   *
   * When an output of the patch renders the producer, its render list reads
   * the port back (score::gfx::PreviewTap): nothing is rendered twice and the
   * preview shows what the output shows. Otherwise the preview gets an
   * offscreen output of its own, which renders the producer's chain, until an
   * output of the patch renders it. Returns an id for the calls below.
   */
  int32_t add_preview(
      port_index producer, QSize size, std::shared_ptr<QRhiReadbackResult> readback);
  void set_preview_producer(int32_t id, port_index producer);
  void set_preview_size(int32_t id, QSize size);
  void remove_preview(int32_t id);
  //! Whether the preview reads from a render list of the patch.
  bool preview_is_tapped(int32_t id) const noexcept;
  //! Whether the preview reads from the shared output of the previews.
  bool preview_is_on_hub(int32_t id) const noexcept;
  //! The consumer took the latest image: the next frame of the list reading
  //! the preview back reads it again. See score::gfx::PreviewTap::wanted.
  void want_preview_frame(int32_t id) const noexcept;
  //! The largest a preview renders and reads back at, on either side.
  static constexpr int max_preview_size = 1024;

  // Synchronously tear down an output node's render list and drop it from the
  // graph. Used by device-owned outputs (e.g. the offscreen BackgroundNode)
  // whose destructor runs during shutdown before the async node-command queue
  // is drained — without this the graph is left with a dangling output pointer
  // and a RenderList referencing an already-freed QRhi.
  void destroyOutput(score::gfx::OutputNode* node);

  //! The outputs the render clocks will drive. An output missing from here
  //! is never rendered, whatever else was built for it.
  std::span<score::gfx::OutputNode* const> outputs() const noexcept;

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
   * When the document is playing, each step first runs the execution for
   * exactly one step of samples, so automations, scripts and media positions
   * are at `(frame + 1) / stepRate()` seconds of execution when frame N is
   * drawn. The count restarts at every play.
   */
  void renderFrames(int frames);

  //! Step used by renderFrames(), in frames per second.
  double stepRate() const noexcept { return m_stepRate; }

  //! A rate above zero puts the execution under the step clock: now if the
  //! document plays, else from its next play, until that play stops.
  void setStepRate(double fps);

  //! Whether the execution currently only advances through renderFrames().
  bool executionStepped() const noexcept;

  /**
   * @brief Whether a render driven from here, or a frame on one of the
   *        outputs' devices, is under way on the calling stack.
   *
   * Every entry point that updates the graph or renders (the render clocks,
   * the free-wheel and vsync callbacks, renderFrames) returns without doing
   * anything while this holds.
   */
  bool renderInProgress() const noexcept;

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
  //! Moves each preview to a render list of the patch, or to its own output.
  void updatePreviews();
  struct Preview
  {
    port_index producer{-1, -1};
    QSize size;
    std::shared_ptr<QRhiReadbackResult> readback;
    std::shared_ptr<score::gfx::PreviewTap> tap;
    //! The node whose previewTaps holds the tap
    int32_t tappedNode{-1};
    //! Its input on the hub, while no output of the patch renders the producer
    int hubInput{-1};
    port_index hubProducer{-1, -1};
    //! The hub rendered the producer into nothing it could read: the preview
    //! keeps its own output until it shows another producer
    bool hubUnreadable{};
    //! An output of its own, when neither can read the producer
    int32_t fallbackId{-1};
    score::gfx::BackgroundNode* fallback{};
    port_index fallbackProducer{-1, -1};
  };
  bool joinPreviewHub(Preview& p);
  void releaseUnusedPreviewHub();
  void leavePreviewHub(Preview& p);
  void startPreviewFallback(Preview& p);
  void stopPreviewFallback(Preview& p);
  //! Renders the producers of the previews no output of the patch renders,
  //! while there are previews
  score::gfx::PreviewHubNode* m_previewHub{};
  int32_t m_previewHubId{-1};
  ossia::flat_map<int32_t, Preview> m_previews;
  int32_t m_nextPreview{1};

  void run_commands();
  void add_preview_output(score::gfx::OutputNode& out);
  void remove_preview_output();
  void add_edge(EdgeSpec e);
  void remove_edge(EdgeSpec e);
  void forgetDeferredEdge(const EdgeSpec& spec) noexcept;
  void remove_node(std::vector<std::unique_ptr<score::gfx::Node>>& nursery, int32_t id);

  void on_no_vsync_timer(score::HighResolutionTimer* self);
  void on_watchdog_timer(score::HighResolutionTimer* self);

  bool frameInProgress() const noexcept;
  bool refuseNestedRender(const char* entry) noexcept;

  const score::DocumentContext& m_context;
  std::atomic_int32_t index{1};
  ossia::hash_map<int32_t, NodePtr> nodes;

  score::gfx::Graph* m_graph{};
  QThread m_thread;

  double m_stepRate{60.};
  int64_t m_stepFrame{};
  int64_t m_stepPlayCount{-1};

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
  //! What update_inputs took from tick_messages, in tick order.
  std::vector<score::gfx::Message> m_drainedMessages;
  std::vector<std::pair<EdgeSpec, int>> m_deferredEdges;

  std::mutex m_disownedLock;
  ossia::flat_set<int32_t> m_disownedPending;
  bool isDisownedPending(int32_t index);

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
  // Each TimerClock owns one shared HighResolutionTimer at a given
  // manualRenderingRate and the coalesced set of outputs driven by it
  // (clock #2, the default); the single DisplayVSyncClock wraps the
  // swap-chain vsync callback (clock #1).
  std::vector<std::unique_ptr<score::gfx::TimerClock>> m_renderClocks;
  std::unique_ptr<score::gfx::DisplayVSyncClock> m_vsyncClock;

  ossia::object_pool<std::vector<score::gfx::gfx_input>> m_buffers;

  AssetTable m_assets;

  score::Timers m_timers;

  int m_renderDepth{};
  int m_nestedRenders{};
};

}
