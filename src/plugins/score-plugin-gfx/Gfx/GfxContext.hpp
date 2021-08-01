#pragma once
#include <Gfx/Graph/Node.hpp>

#include <ossia/dataflow/nodes/media.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/gfx/port_index.hpp>
#include <ossia/network/value/value.hpp>

#include <concurrentqueue.h>
#include <score_plugin_gfx_export.h>
namespace score::gfx
{
struct Graph;
}
namespace score
{
struct DocumentContext;
}
namespace Gfx
{
using port_index = ossia::gfx::port_index;

class GfxExecutionAction;
class SCORE_PLUGIN_GFX_EXPORT GfxContext : public QObject
{
  friend class GfxExecutionAction;

public:
  using NodePtr = std::unique_ptr<score::gfx::Node>;

  explicit GfxContext(const score::DocumentContext& ctx);
  ~GfxContext();

  int32_t register_node(NodePtr node);
  void unregister_node(int32_t idx);

  void recompute_edges();
  void recompute_graph();
  void recompute_connections();

  void update_inputs();
  void updateGraph();

  void send_message(score::gfx::Message&& msg) noexcept {
    tick_messages.enqueue(std::move(msg));
  }

private:
  void run_commands();

  void timerEvent(QTimerEvent*) override;
  const score::DocumentContext& m_context;
  int32_t index{};
  ossia::fast_hash_map<int32_t, NodePtr> nodes;

  score::gfx::Graph* m_graph{};
  QThread m_thread;

  struct Command {
    enum { ADD_NODE, REMOVE_NODE, RELINK } cmd{};
    int32_t index{};
    std::unique_ptr<score::gfx::Node> node;
  };

  moodycamel::ConcurrentQueue<Command> tick_commands;
  moodycamel::ConcurrentQueue<score::gfx::Message> tick_messages;

  std::mutex edges_lock;
  ossia::flat_set<std::pair<port_index, port_index>> new_edges;
  ossia::flat_set<std::pair<port_index, port_index>> edges;
  std::atomic_bool edges_changed{};

  int m_timer{-1};
};

}
