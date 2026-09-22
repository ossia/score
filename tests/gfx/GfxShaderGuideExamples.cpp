// =============================================================================
// Every example in the shader guide renders, pinned by the picture it produces.
// ShaderCorpusTargets.cpp bakes every corpus file for SPIR-V, GLSL, HLSL and
// MSL; this file runs every guide-*.{fs,vs,cs} example and reads pixels back.
// =============================================================================
#include <score_test/Gfx.hpp>

#include "GfxSceneSource.hpp"

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

// Every render helper probes the RHI, which needs a live QGuiApplication:
// QVulkanInstance::supportedApiVersion() segfaults without one.
IsfResult run_isf(score::gfx::GraphicsApi be, const char* f, QSize sz)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(be, {corpus(f)}, sz);
  });
  return r;
}

IsfResult raster(
    score::gfx::GraphicsApi be, const char* cs, const char* vs, const char* fs,
    QSize sz, int frames = 3)
{
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_raster(be, {corpus(cs)}, corpus(vs), corpus(fs), sz, frames);
  });
  return r;
}

// A 0.8-wide square at the origin, both windings so that no cull mode hides it,
// placed by a scene_transform translating it by `x`.
std::shared_ptr<ossia::scene_state> translatedSquare(float x)
{
  constexpr float h = 0.4f;
  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(scene::cpu_buffer(
      {-h, -h, 0, h,  -h, 0, h,  h, 0, -h, -h, 0, h,  h, 0, -h, h, 0,
       -h, -h, 0, h,  h,  0, h,  -h, 0, -h, -h, 0, -h, h, 0, h,  h, 0},
      ossia::buffer_data::usage::vertex_buffer));
  ossia::vertex_attribute pos;
  pos.semantic = ossia::attribute_semantic::position;
  pos.format = ossia::vertex_format::float3;
  pos.byte_stride = 12;
  prim.attributes.push_back(pos);
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 12;
  prim.stable_id = 0x6D1DE003u;
  prim.bounds = {{-h, -h, 0.f}, {h, h, 0.f}};
  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;

  ossia::scene_transform t;
  t.translation[0] = x;
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(t);
  children->push_back(ossia::mesh_component_ptr{std::move(mesh)});
  auto node = std::make_shared<ossia::scene_node>();
  node->children = std::move(children);

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{std::move(node)});
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

struct SceneShot
{
  bool skipped{};
  std::string error;
  ReadbackImage img;
};

SceneShot sceneRaster(score::gfx::GraphicsApi be, float x)
{
  SceneShot s;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int source
        = p.addNode(std::make_unique<scene::StaticSceneNode>(translatedSquare(x)));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(
        corpus("guide-rawraster-scene.vs"), corpus("guide-rawraster-scene.fs"));
    if(source < 0 || flat < 0 || raster < 0)
    {
      s.error = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(source, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    s.img = p.readback(sink);
    if(!s.img.valid())
      s.error = "readback failed";
  });
  return s;
}

// Mean column of the lit pixels, -1 when nothing is lit.
double litCentroidX(const ReadbackImage& img, int& lit)
{
  lit = 0;
  double sum = 0.;
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
      if(img.at(x, y)[0] > 128)
      {
        lit++;
        sum += x;
      }
  return lit ? sum / lit : -1.;
}
}

// Example 1. A plain ISF fragment shader: red ramps left-to-right, green is a
// constant 0.5. Both halves matter -- a constant green that survives proves the
// shader ran, and a red that changes across x proves isf_FragNormCoord is live.
TEST_CASE("guide: a minimal ISF shader ramps across x", "[gfx][l3][guide][isf]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto r = run_isf(be, "guide-isf-basic.fs", {128, 64});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error='" << r.error << "'");
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto left = img.at(4, 32);
  const auto right = img.at(img.width - 5, 32);
  INFO("left rgb " << (int)left[0] << "," << (int)left[1] << "," << (int)left[2]
                   << "  right rgb " << (int)right[0] << "," << (int)right[1]
                   << "," << (int)right[2]);
  CHECK(left[0] < 32);           // ramp starts near 0
  CHECK(right[0] > 200);         // and reaches near 1
  CHECK(right[0] > left[0]);
  CHECK(left[1] > 100);          // constant green: the shader ran
  CHECK(left[1] < 160);
}

// Examples 5 + 2 together: a compute shader produces a 3-vertex triangle, a
// raw-raster pass on the geometry path draws it.
TEST_CASE(
    "guide: a CSF triangle draws through the geometry path",
    "[gfx][l3][guide][rawraster][csf]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto r = raster(
      be, "guide-csf-geometry.cs", "guide-rawraster-geo.vs",
      "guide-rawraster-geo.fs", {128, 128});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend << " error='" << r.error << "'");
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  // The triangle spans x [-0.6, 0.6], y [-0.5, 0.6] in NDC, so its centroid is
  // comfortably inside the frame and the corners are not.
  const auto centre = img.at(img.width / 2, img.height / 2);
  const auto corner = img.at(2, 2);
  INFO("centre rgb " << (int)centre[0] << "," << (int)centre[1] << ","
                     << (int)centre[2] << "  corner rgb " << (int)corner[0]
                     << "," << (int)corner[1] << "," << (int)corner[2]);

  // Something was drawn at the centre...
  const int centreSum = centre[0] + centre[1] + centre[2];
  CHECK(centreSum > 60);
  // ...and the vertex colours interpolated rather than collapsing to one hue.
  CHECK(centre[0] > 10);
  CHECK(centre[1] > 10);
  // ...while the corner, outside the triangle, stayed background.
  CHECK(corner[0] + corner[1] + corner[2] < centreSum);
}

// Example 4. A compute shader writing a storage image, read back through the
// image output it declares. Also pins the top-down texel convention the guide
// calls out: the ramp is on x here, so both ends are sampled on one row.
TEST_CASE("guide: a CSF writes a storage image", "[gfx][l3][guide][csf]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto r = run_isf(be, "guide-csf-image.cs", {64, 64});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  // Needs the GL context the render created: never before it.
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(why);
  INFO("backend=" << r.backend << " error='" << r.error << "'");
  REQUIRE(r.error.empty());
  REQUIRE(!r.outputs.empty());
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto left = img.at(2, 32);
  const auto right = img.at(img.width - 3, 32);
  INFO("left rgb " << (int)left[0] << "," << (int)left[1] << "," << (int)left[2]
                   << "  right rgb " << (int)right[0] << "," << (int)right[1]
                   << "," << (int)right[2]);
  // red ramps up, blue ramps down, green is the constant 0.25 witness.
  CHECK(right[0] > left[0]);
  CHECK(right[2] < left[2]);
  CHECK(left[1] > 40);
  CHECK(left[1] < 90);
}

// Example 6. Frame-to-frame feedback: the same chain rendered for longer must
// come back brighter, which distinguishes a buffer that persists from one
// cleared or reallocated every frame.
TEST_CASE(
    "guide: a read_write attribute accumulates across frames",
    "[gfx][l3][guide][csf][feedback]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto few = raster(
      be, "guide-csf-feedback.cs", "guide-rawraster-geo.vs",
      "guide-rawraster-geo.fs", {96, 96}, 3);
  const auto many = raster(
      be, "guide-csf-feedback.cs", "guide-rawraster-geo.vs",
      "guide-rawraster-geo.fs", {96, 96}, 20);
  if(few.skipped || many.skipped)
    SKIP(few.backend + ": " + few.skip_reason);

  INFO("few error='" << few.error << "' many error='" << many.error << "'");
  REQUIRE(few.error.empty());
  REQUIRE(many.error.empty());
  REQUIRE(!few.outputs.empty());
  REQUIRE(!many.outputs.empty());
  REQUIRE(few.outputs[0].valid());
  REQUIRE(many.outputs[0].valid());

  const auto a = few.outputs[0].at(48, 48);
  const auto b = many.outputs[0].at(48, 48);
  INFO("centre red after 3 frames = " << (int)a[0] << ", after 20 = " << (int)b[0]);
  CHECK(a[0] > 0);       // it drew at all
  CHECK(b[0] > a[0]);    // and the accumulator survived between frames
}

// Example 3. A raw raster on the scene path places each object with
// per_draws.data[draw_id].model, where the scene's transform is baked: the same
// square translated left and right must land left and right of the centre
// (MODEL_MATRIX, identity here, would draw both at the centre).
TEST_CASE(
    "guide: a scene-path raw raster places objects with their model matrix",
    "[gfx][l3][guide][rawraster][scene]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));

  const auto left = sceneRaster(be, -0.8f);
  const auto right = sceneRaster(be, 0.8f);
  if(left.skipped || right.skipped)
    SKIP("backend unavailable");
  // per_draws is a vertex-stage storage buffer. The predicate needs the GL
  // context the renders above created, so it cannot run before them.
  if(const char* why = vertex_storage_buffer_skip_reason(be))
    SKIP(why);
  INFO("left error='" << left.error << "' right error='" << right.error << "'");
  REQUIRE(left.error.empty());
  REQUIRE(right.error.empty());

  int litLeft = 0, litRight = 0;
  const double xLeft = litCentroidX(left.img, litLeft);
  const double xRight = litCentroidX(right.img, litRight);
  INFO(
      "lit " << litLeft << " / " << litRight << ", centroid x " << xLeft << " / "
             << xRight);
  CHECK(litLeft > 20);
  CHECK(litRight > 20);
  CHECK(xLeft < 28.);
  CHECK(xRight > 36.);
}
