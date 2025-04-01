#pragma once
#include <Process/ExecutionAction.hpp>

#include <Gfx/GfxContext.hpp>

#include <ossia/detail/flat_set.hpp>

#include <concurrentqueue.h>
#include <score_plugin_gfx_export.h>

namespace Gfx
{

class SCORE_PLUGIN_GFX_EXPORT GfxExecutionAction final
    : public Execution::ExecutionAction
{
  SCORE_CONCRETE("06f48270-35a4-44d2-929a-e67b8e2904f5")
public:
  explicit GfxExecutionAction(GfxContext& w);

  score::gfx::Message allocateMessage(int inputs);
  void releaseMessage(score::gfx::Message&&);

  void startTick(const ossia::audio_tick_state& st) override;
  void setEdge(port_index source, port_index sink);
  void endTick(const ossia::audio_tick_state& st) override;

  GfxContext* ui{};
  std::vector<EdgeSpec> prev_edges;
  std::vector<EdgeSpec> edges_cache;
  using edge_queue = moodycamel::ConcurrentQueue<EdgeSpec>;
  edge_queue incoming_edges;
};

}
