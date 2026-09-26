// A raw raster's Camera input takes cameras only: a scene there that also
// carries a mesh does not become the raster's geometry.
//
// NodeRenderer::process(port, scene) copies the first mesh of any scene into
// the renderer's single `geometry`, whichever port it came in on. A Cube and a
// Camera wired together to the Camera input must leave the raster drawing its
// own procedural square (rr-camera-inlet-ie, see GfxRawRasterCameraInletIE)
// through that camera: `geometry` stays empty when nothing is on the geometry
// input, and equals the geometry input's spec when a Scene Preprocessor is
// there. Probes: A at column 40 lit and B at column 56 dark (near camera).
#include <score_test/Gfx.hpp>
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

void placeCamera(score::gfx::Node& cam, float z)
{
  setInputs(
      cam, {ossia::value{ossia::vec3f{0.f, 0.f, z}}, ossia::value{ossia::vec3f{0.f, 0.f, 0.f}},
            ossia::value{60.f}, ossia::value{0.1f}, ossia::value{1000.f},
            ossia::value{0.f}});
}

struct Result
{
  bool skipped{};
  std::string error;
  bool rendererFound{};
  bool geometryHasMeshes{};
  bool geometryIsGeometryInput{};
  ReadbackImage img;
};

Result run(score::gfx::GraphicsApi api, bool withGeometryInput)
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
    if(camPort < 0)
    {
      r.error = "no camera input";
      return;
    }

    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    const int cam = p.addNode(procs.make<Threedim::Camera>(ctx));
    placeCamera(*p.node(cam), 3.f);
    p.wire(p.nodeSceneOut(cube, 0), node.input[camPort]);
    p.wire(p.nodeSceneOut(cam, 0), node.input[camPort]);

    if(withGeometryInput)
    {
      const int cube2 = p.addNode(procs.make<Threedim::Cube>(ctx));
      const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
      p.wire(p.nodeSceneOut(cube2, 0), p.nodeSceneIn(flat, 0));
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
    "a mesh in the scene on a raw raster's Camera input is not its geometry",
    "[gfx][raster][camera][e5]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool withGeometryInput = GENERATE(false, true);
  CAPTURE(backend_name(api), withGeometryInput);

  const Result r = run(api, withGeometryInput);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.rendererFound);

  if(withGeometryInput)
    CHECK(r.geometryIsGeometryInput);
  else
    CHECK(!r.geometryHasMeshes);

  INFO("A(x=40)=" << int(r.img.at(40, 32)[0]) << " B(x=56)=" << int(r.img.at(56, 32)[0]));
  CHECK(lit(r.img, 40));
  CHECK(!lit(r.img, 56));
}
