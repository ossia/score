// A Camera wired to a raw raster's Camera input fills its `camera` block.
//
// Every raw raster that reads the injected `camera` block has one extra Scene
// input after its INPUTS ports. With nothing on it the block keeps the identity
// stand-in (no Scene Preprocessor) or the Scene Preprocessor's cameras; with a
// Camera on it the block holds that camera, packed by packCameraUBO, and it
// wins over the Scene Preprocessor's.
//
// rr-camera-inlet-ie draws a square at world x in [0.3, 0.9], y in
// [-0.3, 0.3], z = 0, through VIEWPROJECTION_MATRIX, into a 64x64 sink:
//   identity camera               NDC x in [0.30, 0.90]
//   60-degree camera at (0,0,3)   NDC x in [0.17, 0.52]  (0.6 * cot(30) / 3)
//   60-degree camera at (0,0,10)  NDC x in [0.05, 0.16]
// Probes on the centre row: A at NDC x 0.25 (column 40) and B at NDC x 0.75
// (column 56). Identity lights B only, the near camera lights A only, the far
// camera lights neither.
//
// Registration: see test_gfx_raw_raster_camera_inlet_ie. Threedim::Camera and
// Threedim::Cube are hidden-visibility, so their sources are compiled in.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Threedim/Camera.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

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

void setInputs(score::gfx::Node& n, std::vector<ossia::value> vals)
{
  score::gfx::Message m;
  m.node_id = n.nodeId;
  for(auto& v : vals)
    m.input.push_back(std::move(v));
  n.process(std::move(m));
}

// Camera::ins order: eye, target, fov, near, far, roll.
void placeCamera(score::gfx::Node& cam, float z)
{
  setInputs(
      cam, {ossia::value{ossia::vec3f{0.f, 0.f, z}}, ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
            ossia::value{60.f}, ossia::value{0.1f}, ossia::value{1000.f},
            ossia::value{0.f}});
}

enum class Chain
{
  Unwired,
  InletNear,
  SceneFar,
  SceneFarInletNear,
};

struct Result
{
  bool skipped{};
  std::string error;
  bool cameraPortIsLast{};
  bool cameraPortIsScene{};
  ReadbackImage img;
};

Result run(score::gfx::GraphicsApi api, Chain chain)
{
  Result r;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* document = score::test::new_document(app);
    if(!document)
    {
      r.error = "no document";
      return;
    }
    const score::DocumentContext& ctx = document->context();

    HalpProcesses procs;
    GfxPipeline p;
    const int prod
        = p.addRaster(corpus("rr-camera-inlet-ie.vs"), corpus("rr-camera-inlet-ie.fs"));
    if(prod < 0)
    {
      r.error = "raster: " + p.error();
      return;
    }
    auto& node = *p.isf(prod);
    const int camPort = node.cameraInput();
    r.cameraPortIsLast = camPort >= 0 && camPort == int(node.input.size()) - 1;
    r.cameraPortIsScene
        = camPort >= 0 && node.input[camPort]->type == score::gfx::Types::Scene;
    if(camPort < 0)
    {
      r.error = "no camera input";
      return;
    }

    if(chain == Chain::InletNear || chain == Chain::SceneFarInletNear)
    {
      const int cam = p.addNode(procs.make<Threedim::Camera>(ctx));
      placeCamera(*p.node(cam), 3.f);
      p.wire(p.nodeSceneOut(cam, 0), node.input[camPort]);
    }
    if(chain == Chain::SceneFar || chain == Chain::SceneFarInletNear)
    {
      const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
      const int cam = p.addNode(procs.make<Threedim::Camera>(ctx));
      const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      placeCamera(*p.node(cam), 10.f);
      p.wire(p.nodeSceneOut(cube, 0), p.nodeSceneIn(flat, 0));
      p.wire(p.nodeSceneOut(cam, 0), p.nodeSceneIn(flat, 0));
      p.wire(p.nodeGeometryOut(flat, 0), node.input[0]);
    }

    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(prod, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    r.img = p.readback(sink);
    if(!r.img.valid())
      r.error = "empty readback";
  });
  return r;
}

bool lit(const ReadbackImage& img, int x)
{
  return img.at(x, 32)[0] > 128;
}
}

TEST_CASE(
    "a Camera on a raw raster's Camera input fills its camera block",
    "[gfx][raster][camera][ie]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto chain = GENERATE(
      Chain::Unwired, Chain::InletNear, Chain::SceneFar, Chain::SceneFarInletNear);
  CAPTURE(backend_name(api), int(chain));

  const Result r = run(api, chain);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.cameraPortIsLast);
  CHECK(r.cameraPortIsScene);

  const bool a = lit(r.img, 40);
  const bool b = lit(r.img, 56);
  INFO("A(x=40)=" << int(r.img.at(40, 32)[0]) << " B(x=56)=" << int(r.img.at(56, 32)[0]));
  switch(chain)
  {
    case Chain::Unwired:
      CHECK(!a);
      CHECK(b);
      break;
    case Chain::InletNear:
    case Chain::SceneFarInletNear:
      CHECK(a);
      CHECK(!b);
      break;
    case Chain::SceneFar:
      CHECK(!a);
      CHECK(!b);
      CHECK(lit(r.img, 35));
      break;
  }
}
