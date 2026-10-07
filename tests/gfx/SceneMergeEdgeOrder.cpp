// Several producers into one Scene In merge in the order the sink port keeps
// its edges: by source node id, then output index (Edge::Edge). merge_scenes
// is order-sensitive -- the first contributor carrying an active camera wins
// -- and NodeRenderer stores the deliveries in a map keyed by the source
// Port's address. Merging in map order made the winner depend on where the
// allocator happened to put the two Ports: on macOS a Camera Array and a
// second Camera feeding the same Scene Preprocessor swapped roles from one
// run to the next, and every face of a PER_CUBE_FACE capture was drawn by the
// wrong camera.
//
// Two producers, each with its own active camera; the one created first
// (lower node id) is given the Port at the HIGHER address, so address order
// and node order disagree. Its camera must win, for the merged scene and for
// forEachSceneOnPort, whichever order the deliveries arrive in.
//
// NO GPU: only the CPU process()/merge path is used, as in SceneMergeMemo.cpp.

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <memory>
#include <vector>

using namespace score::gfx;

namespace
{
struct MergeSinkRenderer final : NodeRenderer
{
  using NodeRenderer::NodeRenderer;

  void init(RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override { }
  void release(RenderList&) override { }
  void removeOutputPass(RenderList&, Edge&) override { }
};

struct PlainNode final : Node
{
  NodeRenderer* createRenderer(RenderList&) const noexcept override { return nullptr; }
};

ossia::scene_spec cameraScene(uint64_t camera_id)
{
  auto state = std::make_shared<ossia::scene_state>();
  state->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  state->active_camera_id.value = camera_id;
  state->version = 1;
  ossia::scene_spec s;
  s.state = std::move(state);
  return s;
}
}

TEST_CASE(
    "scenes on one port merge in edge order, not source address order",
    "[gfx][scene]")
{
  const bool firstDeliveredIsLower = GENERATE(true, false);
  CAPTURE(firstDeliveredIsLower);

  PlainNode sink, first, second;
  first.nodeId = 1;
  second.nodeId = 2;

  auto* a = new Port{};
  auto* b = new Port{};
  // `first` gets the port at the higher address.
  Port* firstPort = a < b ? b : a;
  Port* secondPort = a < b ? a : b;
  firstPort->node = &first;
  secondPort->node = &second;
  first.output.push_back(firstPort);
  second.output.push_back(secondPort);
  sink.input.push_back(new Port{&sink});

  Edge e2{secondPort, sink.input[0], Process::CableType::ImmediateGlutton};
  Edge e1{firstPort, sink.input[0], Process::CableType::ImmediateGlutton};
  REQUIRE(sink.input[0]->edges.size() == 2);
  REQUIRE(sink.input[0]->edges[0] == &e1);

  MergeSinkRenderer r{sink};
  const auto s1 = cameraScene(101);
  const auto s2 = cameraScene(202);
  if(firstDeliveredIsLower)
  {
    r.process(0, s1, firstPort);
    r.process(0, s2, secondPort);
  }
  else
  {
    r.process(0, s2, secondPort);
    r.process(0, s1, firstPort);
  }

  REQUIRE(r.scene.state);
  CHECK(r.scene.state->active_camera_id.value == 101);

  std::vector<uint64_t> order;
  r.forEachSceneOnPort(
      0, [&](const ossia::scene_spec& s) { order.push_back(s.state->active_camera_id.value); });
  CHECK(order == std::vector<uint64_t>{101, 202});
}
