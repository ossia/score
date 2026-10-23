// The csf-examples scene test shaders build and draw against the current
// Scene Preprocessor ABI (shader board P39). They were written for the old
// one (48-byte Material, LightGPU, per_draw, cameras.cameras[]) and failed to
// compile or read garbage; they were rewritten for the current layouts, and
// this keeps them there: each one is built as score loads it (includes
// resolved), wired scene -> Scene Preprocessor -> it, and drawn. A layout
// mismatch is a compile or validation error. scene-aux-materials, which reads
// material[0].baseColor directly, is also checked for that colour (magenta =
// material_count read 0). Skips without SCORE_TEST_LIBRARY_ROOT.

#include <score_test/Gfx.hpp>

#include "GfxSceneSource.hpp"
#include "GfxUserLibrary.hpp"

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QFileInfo>

using namespace score::test::gfx;
using scene::StaticSceneNode;

namespace
{
constexpr int kSize = 64;

// A full-screen two-triangle quad with position, normal and texcoord_0, with a
// red unlit material.
std::shared_ptr<ossia::scene_state> redQuad()
{
  auto pos = scene::cpu_buffer(
      {-1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, 1, 0},
      ossia::buffer_data::usage::vertex_buffer);
  auto nrm = scene::cpu_buffer(
      {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
      ossia::buffer_data::usage::vertex_buffer);
  auto uv = scene::cpu_buffer(
      {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1}, ossia::buffer_data::usage::vertex_buffer);

  auto mat = std::make_shared<ossia::material_component>();
  mat->stable_id = 0xF1A0391u;
  mat->unlit = true;
  mat->base_color_factor[0] = 1.f;
  mat->base_color_factor[1] = 0.f;
  mat->base_color_factor[2] = 0.f;
  mat->base_color_factor[3] = 1.f;

  ossia::mesh_primitive prim;
  prim.vertex_buffers = {pos, nrm, uv};
  const auto attr = [](ossia::attribute_semantic s, ossia::vertex_format f, int buf,
                       int stride) {
    ossia::vertex_attribute a;
    a.semantic = s;
    a.format = f;
    a.buffer_index = buf;
    a.byte_stride = stride;
    return a;
  };
  prim.attributes.push_back(
      attr(ossia::attribute_semantic::position, ossia::vertex_format::float3, 0, 12));
  prim.attributes.push_back(
      attr(ossia::attribute_semantic::normal, ossia::vertex_format::float3, 1, 12));
  prim.attributes.push_back(
      attr(ossia::attribute_semantic::texcoord0, ossia::vertex_format::float2, 2, 8));
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = 0xF1A0392u;
  prim.bounds = {{-1.f, -1.f, 0.f}, {1.f, 1.f, 0.f}};
  prim.material = mat;

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;

  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(ossia::mesh_component_ptr(std::move(mesh)));
  auto node = std::make_shared<ossia::scene_node>();
  node->id.value = 39;
  node->children = std::move(children);

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>(
      std::vector<ossia::scene_node_ptr>{node});
  st->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{mat});
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

struct Result
{
  bool skipped = false;
  std::string err;
  ReadbackImage img;
};
}

TEST_CASE(
    "the csf-examples scene test shaders draw against the current scene ABI",
    "[gfx][scene][library][P39]")
{
  const auto name = GENERATE(
      std::string{"fbx-classic-pbr"}, std::string{"fbx-textured-pbr"},
      std::string{"scene-aux-lights"}, std::string{"scene-aux-materials"},
      std::string{"scene-aux-per-draw"}, std::string{"scene-camera-array"},
      std::string{"scene-cubemap-multiview"});
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(name, backend_name(api));

  const QString base = QStringLiteral("csf-testers/tests-scene/") + QString::fromStdString(name);
  const QString fs = library::find(base + ".fs");
  const QString vs = library::find(base + ".vs");
  if(fs.isEmpty() || vs.isEmpty())
    SKIP(library::skip_reason(base + ".fs"));

  Result r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    library::RootGuard root{ctx};
    GfxPipeline p;
    const int hn = p.addNode(std::make_unique<StaticSceneNode>(redQuad()));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vs, fs);
    if(hn < 0 || flat < 0 || raster < 0)
    {
      r.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.err = r.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    r.img = p.readback(sink);
  });
  if(r.skipped)
    SKIP("backend unavailable");
  INFO(r.err);
  REQUIRE(r.err.empty());
  REQUIRE(r.img.valid());

  if(name == "scene-aux-materials")
  {
    const auto c = r.img.center();
    INFO("center " << scene::rgba_string(c));
    CHECK(c[0] > 240);
    CHECK(c[1] < 16);
    CHECK(c[2] < 16);
  }
}
