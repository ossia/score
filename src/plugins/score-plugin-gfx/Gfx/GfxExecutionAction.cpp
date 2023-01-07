#include <Gfx/GfxExecContext.hpp>

#include <ossia/detail/algorithms.hpp>

namespace Gfx
{

GfxExecutionAction::GfxExecutionAction(GfxContext& w)
    : ui{&w}
{
  prev_edges.reserve(100);
  edges_cache.reserve(100);

  // Fixme: do the same for audio & geometry buffers
  for(int i = 0; i < 100; i++)
  {
    std::vector<score::gfx::gfx_input> mbuf;
    mbuf.reserve(16);
    ui->m_buffers.release(std::move(mbuf));
  }
}

score::gfx::Message GfxExecutionAction::allocateMessage(int inputs)
{
  score::gfx::Message m{
      .node_id = {},
      .token = {},
      .input = ui->m_buffers.acquire(),
  };

  m.input.clear();
  m.input.reserve(8);
  return m;
}

void GfxExecutionAction::releaseMessage(score::gfx::Message&& m)
{
  if(m.input.capacity() > 0)
  {
    ui->m_buffers.release(std::move(m.input));
  }
}

void GfxExecutionAction::startTick(const ossia::audio_tick_state& st) { }

void GfxExecutionAction::setEdge(port_index source, port_index sink)
{
  incoming_edges.enqueue({source, sink});
}

void GfxExecutionAction::endTick(const ossia::audio_tick_state& st)
{
  std::atomic_thread_fence(std::memory_order_seq_cst);

  edges_cache.clear();
  edges_cache.reserve(std::max(prev_edges.size(), incoming_edges.size_approx()));

  Edge e;
  while(incoming_edges.try_dequeue(e))
  {
    edges_cache.push_back(e);
  }
  if(edges_cache != prev_edges)
  {
    ossia::remove_duplicates(edges_cache);
    if(edges_cache != prev_edges)
    {
      {
        std::lock_guard l{ui->edges_lock};

        ui->new_edges.container.assign(edges_cache.begin(), edges_cache.end());
      }

      std::swap(edges_cache, prev_edges);
      ui->edges_changed = true;
    }
  }
}
}
