#pragma once
#include <Gfx/GfxContext.hpp>
#include <Process/ExecutionAction.hpp>

#include <ossia/detail/flat_set.hpp>

#include <concurrentqueue.h>

#include <score_plugin_gfx_export.h>

namespace Gfx
{

class SCORE_PLUGIN_GFX_EXPORT GfxExecutionAction final : public Execution::ExecutionAction
{
  SCORE_CONCRETE("06f48270-35a4-44d2-929a-e67b8e2904f5")
public:
  GfxExecutionAction(GfxContext& w);

  void startTick(const ossia::audio_tick_state& st) override;
  void setEdge(port_index source, port_index sink);
  void endTick(const ossia::audio_tick_state& st) override;

  using edge = std::pair<port_index, port_index>;

  GfxContext* ui{};
  std::vector<edge> prev_edges;
  std::vector<edge> edges_cache;
  using edge_queue = moodycamel::ConcurrentQueue<edge>;
  edge_queue incoming_edges;
};

}
