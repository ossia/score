// A Camera wired to a raw raster's Camera input fills its `camera` block.
//
// Every raw raster that reads the injected `camera` block has one extra Scene
// input after its INPUTS ports. With nothing on it the block keeps the identity
// stand-in (no Scene Preprocessor) or the Scene Preprocessor's cameras; with a
// Camera on it the block holds that camera, packed by packCameraUBO, and it
// wins over the Scene Preprocessor's.
//
// rr-camera-inlet draws a square at world x in [0.3, 0.9], y in
// [-0.3, 0.3], z = 0, through VIEWPROJECTION_MATRIX, into a 64x64 sink:
//   identity camera               NDC x in [0.30, 0.90]
//   60-degree camera at (0,0,3)   NDC x in [0.17, 0.52]  (0.6 * cot(30) / 3)
//   60-degree camera at (0,0,10)  NDC x in [0.05, 0.16]
// Probes on the centre row: A at NDC x 0.25 (column 40) and B at NDC x 0.75
// (column 56). Identity lights B only, the near camera lights A only, the far
// camera lights neither.
//
// The Camera input takes cameras only. NodeRenderer::process(port, scene)
// copies the first mesh of any scene into the renderer's single `geometry`,
// whichever port it came in on: a Cube wired next to the Camera on the Camera
// input must leave `geometry` empty when nothing is on the geometry input, and
// equal to the geometry input's spec when a Scene Preprocessor is there.
//
// Threedim::Camera and Threedim::Cube are hidden-visibility, so their sources
// are compiled into the test.
#include <score_test/Gfx.hpp>
#include "GfxHalpNodes.hpp"
#include <score_test/Document.hpp>

#include <Threedim/Camera.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
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
  bool rendererFound{};
  bool geometryHasMeshes{};
  bool geometryIsGeometryInput{};
  ReadbackImage img;
};

Result run(score::gfx::GraphicsApi api, Chain chain, bool meshOnCameraInput = false)
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
        = p.addRaster(corpus("rr-camera-inlet.vs"), corpus("rr-camera-inlet.fs"));
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
      // The mesh is wired before the camera, so it reaches the Camera input
      // first.
      if(meshOnCameraInput)
      {
        const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
        p.wire(p.nodeSceneOut(cube, 0), node.input[camPort]);
      }
      const int cam = p.addNode(procs.make<Threedim::Camera>(ctx));
      placeCamera(*p.node(cam), 3.f);
      p.wire(p.nodeSceneOut(cam, 0), node.input[camPort]);
    }
    if(chain == Chain::SceneFar || chain == Chain::SceneFarInletNear)
    {
      // With a mesh on the Camera input, the geometry input's scene carries no
      // camera: only the geometry routing is under test there.
      const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
      const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      p.wire(p.nodeSceneOut(cube, 0), p.nodeSceneIn(flat, 0));
      if(!meshOnCameraInput)
      {
        const int cam = p.addNode(procs.make<Threedim::Camera>(ctx));
        placeCamera(*p.node(cam), 10.f);
        p.wire(p.nodeSceneOut(cam, 0), p.nodeSceneIn(flat, 0));
      }
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

    for(auto& [list, rn] : node.renderedNodes)
    {
      r.rendererFound = true;
      r.geometryHasMeshes
          = rn->geometry.meshes && !rn->geometry.meshes->meshes.empty();
      const auto* own = rn->findGeometryByPort(0);
      r.geometryIsGeometryInput = own && *own == rn->geometry;
    }
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
    "[gfx][raster][camera]")
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

TEST_CASE(
    "a mesh in the scene on a raw raster's Camera input is not its geometry",
    "[gfx][raster][camera]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const auto chain = GENERATE(Chain::InletNear, Chain::SceneFarInletNear);
  CAPTURE(backend_name(api), int(chain));

  const Result r = run(api, chain, true);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.rendererFound);

  if(chain == Chain::SceneFarInletNear)
    CHECK(r.geometryIsGeometryInput);
  else
    CHECK(!r.geometryHasMeshes);

  INFO("A(x=40)=" << int(r.img.at(40, 32)[0]) << " B(x=56)=" << int(r.img.at(56, 32)[0]));
  CHECK(lit(r.img, 40));
  CHECK(!lit(r.img, 56));
}
