#include <Gfx/GfxExecContext.hpp>
#include <ossia/detail/algorithms.hpp>

namespace Gfx
{

GfxExecutionAction::GfxExecutionAction(GfxContext& w)
  : ui{&w}
{
  prev_edges.reserve(100);
}

void GfxExecutionAction::startTick(const ossia::audio_tick_state& st)
{

}

void GfxExecutionAction::setEdge(port_index source, port_index sink)
{
  incoming_edges.enqueue({source, sink});
}

void GfxExecutionAction::setFixedEdge(port_index source, port_index sink)
{
  fixed_edges.push_back({source, sink});
}

void GfxExecutionAction::unsetFixedEdge(port_index source, port_index sink)
{
  auto it = ossia::find(fixed_edges, edge{source, sink});
  if(it != fixed_edges.end())
    fixed_edges.erase(it);
}

void GfxExecutionAction::endTick(const ossia::audio_tick_state& st)
{
  std::atomic_thread_fence(std::memory_order_seq_cst);

  edges_cache.clear();
  edges_cache.reserve(
        std::max(prev_edges.size(), incoming_edges.size_approx()));

  for(auto& e : fixed_edges)
    edges_cache.push_back(e);

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
}
