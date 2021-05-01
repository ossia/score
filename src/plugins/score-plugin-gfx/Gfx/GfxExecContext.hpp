#pragma once
#include <Gfx/GfxContext.hpp>
#include <Process/ExecutionAction.hpp>

#include <ossia/detail/flat_set.hpp>

#include <concurrentqueue.h>

namespace Gfx
{

class GfxExecutionAction final : public Execution::ExecutionAction
{
  SCORE_CONCRETE("06f48270-35a4-44d2-929a-e67b8e2904f5")
public:
  GfxExecutionAction(gfx_window_context& w)
      : ui{&w}
  {
    prev_edges.reserve(100);
  }
  gfx_window_context* ui{};

  void startTick(const ossia::audio_tick_state& st) override { }

  void setEdge(port_index source, port_index sink)
  {
    incoming_edges.enqueue({source, sink});
  }

  void endTick(const ossia::audio_tick_state& st) override
  {
    std::atomic_thread_fence(std::memory_order_seq_cst);

    edges_cache.clear();
    edges_cache.reserve(
        std::max(prev_edges.size(), incoming_edges.size_approx()));

    edge e;
    while (incoming_edges.try_dequeue(e))
    {
      edges_cache.push_back(e);
    }
    if (edges_cache != prev_edges)
    {
      std::sort(edges_cache.begin(), edges_cache.end());
      if (edges_cache != prev_edges)
      {
        {
          std::lock_guard l{ui->edges_lock};

          ui->new_edges.container.assign(
              edges_cache.begin(), edges_cache.end());
        }

        std::swap(edges_cache, prev_edges);
        ui->edges_changed = true;
      }
    }
  }

  using edge = std::pair<port_index, port_index>;
  std::vector<edge> prev_edges;
  std::vector<edge> edges_cache;
  using edge_queue = moodycamel::ConcurrentQueue<edge>;
  edge_queue incoming_edges;
};

}
