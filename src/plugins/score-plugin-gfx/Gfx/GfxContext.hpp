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

using gfx_input = std::variant<ossia::value, ossia::audio_vector>;

struct gfx_message
{
  int32_t node_id{};
  ossia::token_request token{};
  std::vector<std::vector<gfx_input>> inputs;
};

//! Goes from ossia execution graph messages,
//! to data in the shader uniforms / textures
struct gfx_view_node
{
  std::unique_ptr<score::gfx::ProcessNode> impl;

  void process(const ossia::token_request& tk);
  void process(int32_t port, const ossia::value& v);
  void process(int32_t port, const ossia::audio_vector& v);
};

class gfx_exec_node;
class GfxExecutionAction;
class SCORE_PLUGIN_GFX_EXPORT GfxContext : public QObject
{
  friend class gfx_exec_node;
  friend class GfxExecutionAction;

public:
  explicit GfxContext(const score::DocumentContext& ctx);
  ~GfxContext();

  int32_t register_node(std::unique_ptr<score::gfx::ProcessNode> node);
  void unregister_node(int32_t idx);

  void recompute_edges();
  void recompute_graph();
  void recompute_connections();

  void update_inputs();
  void updateGraph();

  void timerEvent(QTimerEvent*) override;

private:
  const score::DocumentContext& m_context;
  int32_t index{};
  ossia::fast_hash_map<int32_t, gfx_view_node> nodes;

  score::gfx::Graph* m_graph{};
  QThread m_thread;

  moodycamel::ConcurrentQueue<gfx_message> tick_messages;

  std::mutex edges_lock;
  ossia::flat_set<std::pair<port_index, port_index>> new_edges;
  ossia::flat_set<std::pair<port_index, port_index>> edges;
  std::atomic_bool edges_changed{};

  int m_timer{-1};
  bool must_recompute = false;
};

}
