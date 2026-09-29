// A Structure Synth program reaches the screen in a render that does not run
// the event loop between frames, as GfxContext::renderFrames does for an
// offline render or a grab.
//
// The program is a control value and arrives with the node's first message.
// It is built on a worker thread; the mesh reaches the node at one of its
// next ticks, not through the event loop. In a render driven by the step
// clock, the tick waits for the build: the program is drawn in the very frame
// that carries it, however slow the worker. Here it is `box`, a unit cube drawn
// by ModelDisplay with its built-in lighting, so the cube's pixels must cover
// a good part of the target.
#include <score_test/Gfx.hpp>
#include "GfxHalpNodes.hpp"
#include <score_test/Document.hpp>

#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>
#include <Threedim/StructureSynth.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <chrono>
#include <thread>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct Shot
{
  bool skipped{};
  std::string error;
  int lit{};
  int total{};
};

// The values the execution engine sends a freshly loaded Structure Synth: every
// control, in inlet order (Program, Position, Rotation, Scale).
score::gfx::Message loadedControls(int32_t node_id, const std::string& program)
{
  score::gfx::Message m;
  m.node_id = node_id;
  m.input.resize(4);
  m.input[0] = ossia::value{program};
  m.input[1] = ossia::value{ossia::vec3f{0.f, 0.f, 0.f}};
  m.input[2] = ossia::value{ossia::vec3f{0.f, 0.f, 0.f}};
  m.input[3] = ossia::value{ossia::vec3f{1.f, 1.f, 1.f}};
  return m;
}

Shot render(score::gfx::GraphicsApi api, bool stepped)
{
  Shot s;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      s.error = "no document";
      return;
    }
    HalpProcesses procs;
    GfxPipeline p;
    auto ssNode = procs.make<Threedim::StrucSynth>(doc->context());
    auto* ss = ssNode.get();
    const int synth = p.addNode(std::move(ssNode));

    auto md = std::make_unique<score::gfx::ModelDisplayNode>();
    md->texture_projection = 6;
    md->position = {2.f, 1.5f, 3.f};
    md->center = {0.5f, 0.5f, 0.5f};
    md->fov = 45.f;
    auto* mdNode = md.get();
    const int display = p.addNode(std::move(md));
    if(synth < 0 || display < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(synth, 0), mdNode->input[1]);
    const int sink = p.addSink({96, 96});
    p.wire(p.nodeImageOut(display, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }

    // As GfxContext::renderFrames sets it for a render under the step clock.
    if(stepped)
      for(auto& rl : p.graph().renderLists())
        if(rl)
          rl->dateFromStepClock = true;

    ss->process(loadedControls(ss->nodeId, "box"));

    // Frames with the event loop not running in between, until the worker's
    // mesh shows up (it takes well under a second); stepped: one frame.
    for(int frame = 0; frame < (stepped ? 1 : 200) && s.lit == 0; frame++)
    {
      p.render(1);
      const ReadbackImage img = p.readback(sink);
      if(!img.valid())
      {
        s.error = "empty readback";
        return;
      }
      for(int y = 0; y < img.height; y++)
        for(int x = 0; x < img.width; x++)
        {
          const auto px = img.at(x, y);
          if(px[0] + px[1] + px[2] > 30)
            s.lit++;
        }
      s.total = img.width * img.height;
      if(s.lit == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  });
  return s;
}
}

TEST_CASE("A Structure Synth program is drawn", "[gfx][threedim][ssynth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const bool stepped = GENERATE(false, true);
  CAPTURE(stepped);

  const auto s = render(api, stepped);
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());
  INFO("lit " << s.lit << " of " << s.total);
  CHECK(s.lit > s.total / 10);
  CHECK(s.lit < s.total);
}
