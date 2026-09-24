// A Structure Synth program reaches the screen.
//
// The program is a control value and arrives with the node's first message.
// Evaluated on a worker thread, its mesh was applied back onto the object from
// the event loop, which an offline render (GfxContext::renderFrames, the grab
// harness) does not run between frames: a loaded score drew nothing in every
// frame it rendered. The first program is now evaluated when it arrives. Here
// it is `box`, a unit cube drawn by ModelDisplay with its built-in lighting, so
// the cube's pixels must cover a good part of the target.
//
// Registration: see the test_gfx_structure_synth_fixg target.
#include <score_test/Gfx.hpp>
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

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct HalpProcesses
{
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  int next = 1;
  template <typename T>
  std::unique_ptr<score::gfx::Node> make(const score::DocumentContext& ctx)
  {
    auto model = std::make_unique<oscr::ProcessModel<T>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{next}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));
    return std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, next++, ctx}};
  }
};

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

Shot render(score::gfx::GraphicsApi api)
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

    ss->process(loadedControls(ss->nodeId, "box"));

    // As GfxContext::renderFrames does for an offline render: frames back to
    // back, the event loop not running in between.
    p.render(4);
    const ReadbackImage img = p.readback(sink);
    if(img.valid())
    {
      for(int y = 0; y < img.height; y++)
        for(int x = 0; x < img.width; x++)
        {
          const auto px = img.at(x, y);
          if(px[0] + px[1] + px[2] > 30)
            s.lit++;
        }
      s.total = img.width * img.height;
    }
    if(!img.valid())
      s.error = "empty readback";
  });
  return s;
}
}

TEST_CASE("A Structure Synth program is drawn", "[gfx][threedim][ssynth]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto s = render(api);
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());
  INFO("lit " << s.lit << " of " << s.total);
  CHECK(s.lit > s.total / 10);
}
