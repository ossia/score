// A Model Display drawing a geometry assembled from GPU buffers, the chain of
// the "extract attribute" example: Plane -> Extract buffer (Position) ->
// Buffers to geometry -> Model Display, textured by a solid colour.
//
// A full rebuild of the render list releases and initialises every renderer
// again, so each GPU buffer the chain passes along is replaced; an output resize
// rebuilds only what follows the output size. The plane must still be drawn
// after either, and the Mode control must still pick the primitive topology.
//
// With a colour attribute and neither texture coordinates nor normals -- the
// example's goblet, coloured from a texture turned into a buffer -- Tex. Proj.
// must still pick the projections such a mesh can take.
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

#include <cstdlib>
#include <functional>
#include <sstream>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;
using score::test::gfx::isf::corpus;

namespace
{
// Model Display's inputs as its executor numbers them: 0 texture, 1 geometry,
// then the controls; Tex. Proj. is 7, Mode is 8.
constexpr int TexProjInput = 7;
constexpr int ModeInput = 8;
enum DrawMode
{
  Triangles = 0,
  Points = 1,
  Lines = 2
};

// Tex. Proj. values, as the combo box stores them.
enum TexProj
{
  ViewSpace = 4,
  Barycentric = 5,
  Light = 6,
  VertexColor = 7
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

// Share of the image showing the solid texture (0.25, 0.5, 0.75): the
// texture-sampling projections paint the plane with it, while its vertex
// colours, the positions of a plane at z = 0, have no blue at all.
double textured(const ReadbackImage& img)
{
  if(!img.valid())
    return -1.;
  int n = 0;
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
    {
      const auto p = img.at(x, y);
      if(std::abs(p[0] - 64) < 16 && std::abs(p[1] - 128) < 16
         && std::abs(p[2] - 191) < 16)
        n++;
    }
  return double(n) / (img.width * img.height);
}

void setControl(score::gfx::Node& md, int input, int value)
{
  score::gfx::Message m;
  m.node_id = md.nodeId;
  m.input.resize(input + 1);
  m.input[input] = ossia::value{value};
  md.process(std::move(m));
}

void setDrawMode(score::gfx::Node& md, int mode)
{
  setControl(md, ModeInput, mode);
}

// Attribute 0 is the position; with `vertexColors`, attribute 1 reads the same
// buffer again as the colour.
std::vector<ossia::value> buffersToGeometryInputs(bool vertexColors)
{
  std::vector<ossia::value> v(8); // Buffer 0..7 are cabled, not values.
  for(int attr = 0; attr < 8; attr++)
  {
    const bool used = attr == 0 || (attr == 1 && vertexColors);
    v.push_back(used ? 0 : -1); // buffer
    v.push_back(0);             // offset
    v.push_back(0);             // stride
    v.push_back(used ? 1 : 0);  // format: Float3
    v.push_back(std::string(attr == 0 ? "position" : used ? "color" : ""));
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
  std::vector<double> textured;
};

// Renders the chain, then runs each step and records the coverage after it.
// `textureSize`, when valid, gives the texture inlet a size of its own.
Result run(
    score::gfx::GraphicsApi api,
    const std::vector<std::function<void(GfxPipeline&, int sink, score::gfx::Node&)>>&
        steps,
    QSize sinkSize = {96, 96}, QSize textureSize = {}, bool vertexColors = false,
    int texProj = 0)
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
    mdNode->texture_projection = texProj;
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
    setInputs(*b2g, buffersToGeometryInputs(vertexColors));
    const auto shoot = [&] {
      p.render(6);
      const auto img = p.readback(sink);
      r.coverage.push_back(coverage(img));
      r.textured.push_back(textured(img));
    };
    shoot();
    for(auto& step : steps)
    {
      step(p, sink, *md);
      shoot();
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
auto texProj(int t)
{
  return [t](GfxPipeline&, int, score::gfx::Node& md) {
    setControl(md, TexProjInput, t);
  };
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

TEST_CASE(
    "a Model Display's Tex. Proj. applies to a geometry with vertex colours only",
    "[gfx][threedim][modeldisplay]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // Starts on Light, which needs normals: the vertex colours stand in for it.
  // Lines are one pixel wide in the Model Display's own target: a sink of the
  // same aspect ratio keeps them when it is scaled down.
  const auto r = run(
      api,
      {texProj(ViewSpace), texProj(VertexColor), texProj(Barycentric), texProj(Light),
       texProj(ViewSpace), mode(Lines), mode(Triangles), texProj(VertexColor),
       texProj(Barycentric)},
      {128, 72}, {}, true, Light);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.textured.size() == 10);
  std::ostringstream trace;
  for(std::size_t i = 0; i < r.textured.size(); i++)
    trace << " [" << i << "] coverage " << r.coverage[i] << " textured "
          << r.textured[i];
  INFO(trace.str());

  const auto vertexColours = [&](int i) {
    CHECK(r.textured[i] < 0.01);
  };
  const auto texture = [&](int i) {
    CHECK(r.textured[i] > 0.1);
  };
  vertexColours(0); // Light
  texture(1);       // View-space
  vertexColours(2); // Color
  texture(3);       // Barycentric
  vertexColours(4); // Light
  texture(5);       // View-space
  // Lines keep the projection and light a fraction of what the triangles fill.
  CHECK(r.textured[6] > 0.);
  CHECK(r.textured[6] < r.textured[5] / 2);
  texture(7);       // back to triangles
  vertexColours(8); // Color
  texture(9);       // Barycentric
}

TEST_CASE(
    "a Model Display's Mode switches back and forth on a vertex-coloured geometry",
    "[gfx][threedim][modeldisplay]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // See above for the sink size.
  const auto r = run(
      api,
      {mode(Lines), mode(Triangles), mode(Points), mode(Lines), mode(Points),
       mode(Triangles), mode(Lines)},
      {128, 72}, {}, true, ViewSpace);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.textured.size() == 8);
  std::ostringstream trace;
  for(std::size_t i = 0; i < r.textured.size(); i++)
    trace << " [" << i << "] textured " << r.textured[i];
  INFO(trace.str());

  const double filled = r.textured[0];
  CHECK(filled > 0.1);
  for(int i : {2, 6})
    CHECK(r.textured[i] == filled); // triangles again: the same image
  for(int i : {1, 4, 7})
  {
    CHECK(r.textured[i] > 0.);
    CHECK(r.textured[i] < filled / 2); // lines
  }
  for(int i : {3, 5})
    CHECK(r.textured[i] < r.textured[1]); // points light fewer pixels than lines
}
