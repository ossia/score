// ProcessNode's EVENT inputs and the shared FRAMEINDEX step, on the CPU.
//
// An EVENT input fires for exactly one frame: on an impulse (which the
// generic port writer ignores) fireEventPort writes 1 and bumps the material
// generation; resetEventPortsAfterFrame writes 0 back once the frame's
// material is staged and reports whether anything had fired, so the renderer
// uploads the 0 on the next frame.
//
// advanceFrameIndex is the once-per-RenderList-frame FRAMEINDEX step every
// ISF / VSA / CSF / raw raster renderer runs from update(), which is called
// once per output edge: a node with several outlets must still count frames,
// not edges, and a fresh node starts at 0.
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderedISFUtils.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct EventNode final : score::gfx::ProcessNode
{
  int event{};
  int plain{};

  EventNode()
  {
    input.push_back(new score::gfx::Port{this, &event, score::gfx::Types::Int, {}});
    input.push_back(new score::gfx::Port{this, &plain, score::gfx::Types::Int, {}});
    m_event_ports.push_back(&event);
  }

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return nullptr;
  }

  using ProcessNode::fireEventPort;
};
}

TEST_CASE("an impulse fires an event input for one frame", "[gfx][event]")
{
  EventNode n;
  int64_t seen = 0;
  REQUIRE_FALSE(n.hasMaterialChanged(seen));

  CHECK(n.fireEventPort(0, ossia::impulse{}));
  CHECK(n.event == 1);
  CHECK(n.hasMaterialChanged(seen));

  CHECK(n.resetEventPortsAfterFrame());
  CHECK(n.event == 0);
  CHECK_FALSE(n.resetEventPortsAfterFrame());
}

TEST_CASE("fireEventPort leaves everything else to the generic writer", "[gfx][event]")
{
  EventNode n;
  int64_t seen = 0;

  CHECK_FALSE(n.fireEventPort(0, ossia::value{true}));
  CHECK_FALSE(n.fireEventPort(1, ossia::impulse{}));
  CHECK_FALSE(n.fireEventPort(2, ossia::impulse{}));
  CHECK_FALSE(n.fireEventPort(-1, ossia::impulse{}));
  CHECK(n.event == 0);
  CHECK(n.plain == 0);
  CHECK_FALSE(n.hasMaterialChanged(seen));
}

TEST_CASE("FRAMEINDEX advances once per frame, from 0", "[gfx][frameindex]")
{
  int32_t index = 0;
  int64_t last = -1;

  // First frame of a fresh node: index stays 0, the first edge is first.
  CHECK(score::gfx::advanceFrameIndex(index, last, 10));
  CHECK(index == 0);
  // A second output edge in the same frame.
  CHECK_FALSE(score::gfx::advanceFrameIndex(index, last, 10));
  CHECK(index == 0);

  CHECK(score::gfx::advanceFrameIndex(index, last, 11));
  CHECK(index == 1);
  CHECK_FALSE(score::gfx::advanceFrameIndex(index, last, 11));
  CHECK(score::gfx::advanceFrameIndex(index, last, 12));
  CHECK(index == 2);
}

TEST_CASE("FRAMEINDEX keeps counting across a renderer rebuild", "[gfx][frameindex]")
{
  // The index lives on the node, the last frame on the renderer: a new
  // renderer for a node that already rendered continues from there.
  int32_t index = 7;
  int64_t last = -1;
  CHECK(score::gfx::advanceFrameIndex(index, last, 0));
  CHECK(index == 8);
}
