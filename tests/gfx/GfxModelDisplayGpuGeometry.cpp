// A Model Display drawing a geometry assembled from GPU buffers, the chain of
// the "extract attribute" example: Plane -> Extract buffer (Position) ->
// Buffers to geometry -> Model Display, textured by a solid colour.
//
// A full rebuild of the render list releases and initialises every renderer
// again, so each GPU buffer the chain passes along is replaced; an output resize
// rebuilds only what follows the output size. The plane must still be drawn
// after either, and the Mode control must still pick the primitive topology.
#include "GfxHalpNodes.hpp"
#include "IsfTestCommon.hpp"

#include <score_test/Document.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <Threedim/BufferToGeometry2.hpp>
#include <Threedim/GeometryToBuffer.hpp>
#include <Threedim/ModelDisplay/ModelDisplayNode.hpp>
#include <Threedim/Primitive.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <functional>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
// Model Display's inputs as its executor numbers them: 0 texture, 1 geometry,
// then the controls; Mode is 8.
constexpr int ModeInput = 8;
enum DrawMode
{
  Triangles = 0,
  Points = 1,
  Lines = 2
};

// A 2x2 grid is one quad: two triangles, six vertices once de-indexed.
constexpr int PlaneVertices = 6;

double coverage(const ReadbackImage& img)
{
  if(!img.valid())
    return -1.;
  int lit = 0;
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
    {
      const auto p = img.at(x, y);
      if(p[0] + p[1] + p[2] > 60)
        lit++;
    }
  return double(lit) / (img.width * img.height);
}

void setDrawMode(score::gfx::Node& md, int mode)
{
  score::gfx::Message m;
  m.node_id = md.nodeId;
  m.input.resize(ModeInput + 1);
  m.input[ModeInput] = ossia::value{mode};
  md.process(std::move(m));
}

std::vector<ossia::value> buffersToGeometryInputs()
{
  std::vector<ossia::value> v(8); // Buffer 0..7 are cabled, not values.
  for(int attr = 0; attr < 8; attr++)
  {
    v.push_back(attr == 0 ? 0 : -1); // buffer
    v.push_back(0);                  // offset
    v.push_back(0);                  // stride
    v.push_back(attr == 0 ? 1 : 0);  // format: Float3
    v.push_back(std::string(attr == 0 ? "position" : ""));
    v.push_back(false); // instanced
  }
  v.push_back(-1);             // index buffer
  v.push_back(0);              // index format
  v.push_back(0);              // index offset
  v.push_back(PlaneVertices); // vertices
  v.push_back(1);              // instances
  v.push_back(0);              // topology: triangles
  v.push_back(0);              // cull: none
  v.push_back(0);              // front face: CCW
  return v;
}

struct Result
{
  bool skipped{};
  std::string error;
  std::vector<double> coverage;
};

// Renders the chain, then runs each step and records the coverage after it.
// `textureSize`, when valid, gives the texture inlet a size of its own.
Result run(
    score::gfx::GraphicsApi api,
    const std::vector<std::function<void(GfxPipeline&, int sink, score::gfx::Node&)>>&
        steps,
    QSize sinkSize = {96, 96}, QSize textureSize = {})
{
  Result r;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      r.error = "no document";
      return;
    }
    HalpProcesses procs;
    GfxPipeline p;
    const int plane = p.addNode(procs.make<Threedim::Plane>(doc->context()));
    auto extractNode = procs.make<Threedim::ExtractBuffer>(doc->context());
    auto* extract = extractNode.get();
    const int ex = p.addNode(std::move(extractNode));
    auto b2gNode = procs.make<Threedim::BuffersToGeometry2>(doc->context());
    auto* b2g = b2gNode.get();
    const int toGeom = p.addNode(std::move(b2gNode));
    const int image = p.addIsf(corpus("isf-solid-color-opaque.fs"));
    auto mdNode = std::make_unique<score::gfx::ModelDisplayNode>();
    mdNode->position = {0.f, 0.f, 1.5f};
    mdNode->center = {0.f, 0.f, 0.f};
    mdNode->fov = 60.f;
    if(textureSize.isValid())
      mdNode->renderTargetSpecs[0].size
          = ossia::texture_size{textureSize.width(), textureSize.height()};
    auto* md = mdNode.get();
    const int display = p.addNode(std::move(mdNode));
    if(plane < 0 || ex < 0 || toGeom < 0 || image < 0 || display < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    auto* planeOut = p.nodeGeometryOut(plane, 0);
    auto* bufOut = p.nodeBufferOut(ex, 0);
    auto* bufIn = nth_buffer_input(*b2g, 0);
    auto* geoOut = p.nodeGeometryOut(toGeom, 0);
    if(!planeOut || !bufOut || !bufIn || !geoOut || extract->input.empty())
    {
      r.error = "ports missing";
      return;
    }
    p.wire(planeOut, extract->input[0]);
    p.wire(bufOut, bufIn);
    p.wire(geoOut, md->input[1]);
    p.wire(p.imageOut(image, 0), md->input[0]);
    const int sink = p.addSink(sinkSize);
    p.wire(p.nodeImageOut(display, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    setInputs(
        *p.node(plane), {ossia::vec3f{0.f, 0.f, 0.f}, ossia::vec3f{0.f, 0.f, 0.f},
                         ossia::vec3f{1.f, 1.f, 1.f}, 2, 2});
    setInputs(*b2g, buffersToGeometryInputs());
    p.render(6);
    r.coverage.push_back(coverage(p.readback(sink)));
    for(auto& step : steps)
    {
      step(p, sink, *md);
      p.render(6);
      r.coverage.push_back(coverage(p.readback(sink)));
    }
  });
  return r;
}

auto resize(QSize sz)
{
  return [sz](GfxPipeline& p, int sink, score::gfx::Node&) { p.resizeSink(sink, sz); };
}
// Inside a frame, as the render loop rebuilds a list: what release() hands to
// deleteLater() outlives the rebuild, so no new resource takes its address.
auto rebuild()
{
  return [](GfxPipeline& p, int, score::gfx::Node&) {
    for(auto& rl : p.graph().renderLists())
    {
      score::gfx::OffscreenFrame frame{*rl->state.rhi};
      rl->maybeRebuild(true);
    }
  };
}
auto mode(int m)
{
  return [m](GfxPipeline&, int, score::gfx::Node& md) { setDrawMode(md, m); };
}
}

TEST_CASE(
    "a Model Display keeps drawing a GPU-buffer geometry across rebuilds and resizes",
    "[gfx][threedim][modeldisplay][resize]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = run(
      api, {resize({128, 72}), rebuild(), resize({96, 96}), rebuild(), resize({64, 80})});
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.coverage.size() == 6);
  INFO(
      "coverage " << r.coverage[0] << " " << r.coverage[1] << " " << r.coverage[2]
                  << " " << r.coverage[3] << " " << r.coverage[4] << " "
                  << r.coverage[5]);
  for(double c : r.coverage)
    CHECK(c > 0.1);
}

TEST_CASE(
    "a Model Display's Mode picks the primitive topology after a rebuild",
    "[gfx][threedim][modeldisplay][resize]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = run(
      api,
      {resize({128, 72}), rebuild(), mode(Points), mode(Lines), mode(Triangles)});
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.coverage.size() == 6);
  INFO(
      "coverage " << r.coverage[0] << " " << r.coverage[1] << " " << r.coverage[2]
                  << " " << r.coverage[3] << " " << r.coverage[4] << " "
                  << r.coverage[5]);
  CHECK(r.coverage[1] > 0.1);
  CHECK(r.coverage[2] > 0.1);
  // Points and lines light a few pixels of what the triangles fill.
  CHECK(r.coverage[3] < r.coverage[2] / 4);
  CHECK(r.coverage[4] < r.coverage[2] / 2);
  CHECK(r.coverage[4] > r.coverage[3]);
  CHECK(r.coverage[5] > 0.1);
}

TEST_CASE(
    "a Model Display's projection follows an output resize its texture inlet ignores",
    "[gfx][threedim][modeldisplay][resize]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // The inlet keeps its size, so nothing re-initialises the Model Display. The
  // plane keeps its height in pixels on a target twice as wide: it covers half
  // of it, where a projection kept at the old aspect ratio stretches it across.
  const auto r = run(api, {resize({192, 96})}, {96, 96}, {32, 32});
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.coverage.size() == 2);
  INFO("coverage " << r.coverage[0] << " " << r.coverage[1]);
  CHECK(r.coverage[0] > 0.1);
  CHECK(r.coverage[1] > 0.4 * r.coverage[0]);
  CHECK(r.coverage[1] < 0.6 * r.coverage[0]);
}
