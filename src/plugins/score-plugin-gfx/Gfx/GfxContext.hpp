#pragma once
#include <Gfx/Graph/Node.hpp>

#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/buffer_pool.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/mutex.hpp>
#include <ossia/detail/variant.hpp>
#include <ossia/detail/small_flat_map.hpp>
#include <ossia/gfx/port_index.hpp>
#include <ossia/network/value/value.hpp>
#include <score/tools/Timers.hpp>
#include <concurrentqueue.h>
#include <score_plugin_gfx_export.h>
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

using EdgeSpec = std::pair<port_index, port_index>;
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

  void recompute_edges();
  void recompute_graph();
  void recompute_connections();

  void update_inputs();
  void updateGraph();

  void send_message(score::gfx::Message&& msg) noexcept
  {
    tick_messages.enqueue(std::move(msg));
  }

private:
  void run_commands();
  void add_preview_output(score::gfx::OutputNode& out);
  void remove_preview_output();
  void add_edge(EdgeSpec e);
  void remove_edge(EdgeSpec e);
  void remove_node(std::vector<std::unique_ptr<score::gfx::Node>>& nursery, int32_t id);

  void on_no_vsync_timer(score::HighResolutionTimer* self);
  void on_watchdog_timer(score::HighResolutionTimer* self);
  void on_manual_timer(score::HighResolutionTimer* self);
  const score::DocumentContext& m_context;
  std::atomic_int32_t index{1};
  ossia::hash_map<int32_t, NodePtr> nodes;

  score::gfx::Graph* m_graph{};
  QThread m_thread;

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

  std::mutex edges_lock;
  ossia::flat_set<EdgeSpec> new_edges TS_GUARDED_BY(edges_lock);
  ossia::flat_set<EdgeSpec> edges;
  ossia::flat_set<EdgeSpec> preview_edges;
  std::atomic_bool edges_changed{};

  score::HighResolutionTimer* m_no_vsync_timer{};
  score::HighResolutionTimer* m_watchdog_timer{};

  ossia::small_flat_map<score::HighResolutionTimer*, ossia::flat_set<score::gfx::OutputNode*>, 8> m_manualTimers;

  ossia::object_pool<std::vector<score::gfx::gfx_input>> m_buffers;

  score::Timers m_timers;
};

}
