// A texture's wrap mode reaches the pixel through the shipped material preset.
//
// The pool samples every array with one repeat sampler and publishes each
// texture's own wrap mode in scene_material_wrap; the preset's sampling
// helper applies it to the coordinate. This drives the real chain -- scene,
// ScenePreprocessor, classic_pbr_full -- with an unlit quad whose U runs from
// 0 to 2 across it, textured left half red, right half blue. At U = 1.25
// repeat reads red (0.25), clamp-to-edge reads blue (the right edge), and
// mirror reads blue (0.75).
//
// The preset lives in the user library; without SCORE_TEST_LIBRARY_ROOT the
// test skips.
#include "GfxSceneSource.hpp"
#include "GfxUserLibrary.hpp"

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <score_test/Gfx.hpp>

using namespace score::test::gfx;
using scene::cpu_buffer;
using scene::rgba_string;

namespace
{
constexpr int kSize = 64;
const QString kFullFrag = QStringLiteral("presets/rasterizers/classic_pbr_full.frag");

ossia::material_component_ptr splitMaterial(ossia::texture_address_mode wrap)
{
  QImage img(64, 64, QImage::Format_RGBA8888);
  img.fill(QColor(255, 0, 0, 255));
  for(int y = 0; y < img.height(); ++y)
    for(int x = img.width() / 2; x < img.width(); ++x)
      img.setPixelColor(x, y, QColor(0, 0, 255, 255));
  auto m = std::make_shared<ossia::material_component>();
  m->stable_id = 0x3A7E0001u;
  m->unlit = true;
  m->base_color_texture.source = scene::png_source(img);
  m->base_color_texture.sampler.wrap_s = wrap;
  m->base_color_texture.sampler.wrap_t = wrap;
  return m;
}

std::shared_ptr<ossia::scene_state> makeState(ossia::material_component_ptr mat)
{
  constexpr float e = 0.8f;
  auto pos = cpu_buffer(
      {-e, -e, 0, e, -e, 0, e, e, 0, -e, -e, 0, e, e, 0, -e, e, 0},
      ossia::buffer_data::usage::vertex_buffer);
  auto uv = cpu_buffer(
      {0, 0, 2, 0, 2, 1, 0, 0, 2, 1, 0, 1}, ossia::buffer_data::usage::vertex_buffer);

  ossia::mesh_primitive prim;
  prim.vertex_buffers = {pos, uv};
  ossia::vertex_attribute p;
  p.semantic = ossia::attribute_semantic::position;
  p.format = ossia::vertex_format::float3;
  p.buffer_index = 0;
  p.byte_stride = 12;
  prim.attributes.push_back(p);
  ossia::vertex_attribute t;
  t.semantic = ossia::attribute_semantic::texcoord0;
  t.format = ossia::vertex_format::float2;
  t.buffer_index = 1;
  t.byte_stride = 8;
  prim.attributes.push_back(t);
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = 0x3A7E0002u;
  prim.bounds = {{-e, -e, 0.f}, {e, e, 0.f}};
  prim.material = mat;

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(ossia::mesh_component_ptr(std::move(mesh)));
  auto root = std::make_shared<ossia::scene_node>();
  root->children = std::move(children);
  auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  roots->push_back(std::move(root));

  auto st = std::make_shared<ossia::scene_state>();
  st->roots = std::move(roots);
  st->materials = std::make_shared<std::vector<ossia::material_component_ptr>>(
      std::vector<ossia::material_component_ptr>{mat});
  st->version = 1;
  st->dirty_index = 1;
  return st;
}

struct Probe
{
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> inside{}, beyond{};
};

Probe render(score::gfx::GraphicsApi api, ossia::texture_address_mode wrap)
{
  Probe out;
  const QString fs = library::find(kFullFrag);
  const QString vs = fs.chopped(4) + QStringLiteral("vert");
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    library::RootGuard root{ctx};

    GfxPipeline p;
    const int hn = p.addNode(
        std::make_unique<scene::StaticSceneNode>(makeState(splitMaterial(wrap))));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(vs, fs);
    if(hn < 0 || flat < 0 || raster < 0)
    {
      out.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.err = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(5);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      out.err = "readback failed";
      return;
    }
    // The quad spans px 6.4..57.6 with U = 0..2 across it: px 13 is U 0.26,
    // px 38 is U 1.23.
    out.inside = img.at(13, kSize / 2);
    out.beyond = img.at(38, kSize / 2);
  });
  return out;
}

}

TEST_CASE(
    "classic_pbr_full applies a texture's wrap mode",
    "[gfx][presets][material][sampler]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(library::find(kFullFrag).isEmpty())
    SKIP(library::skip_reason(kFullFrag));

  const auto repeat = render(api, ossia::texture_address_mode::REPEAT);
  if(repeat.skipped)
    SKIP("backend unavailable");
  const auto clamp = render(api, ossia::texture_address_mode::CLAMP_TO_EDGE);
  const auto mirror = render(api, ossia::texture_address_mode::MIRROR);

  INFO(
      "repeat " << rgba_string(repeat.inside) << " " << rgba_string(repeat.beyond)
                << " | clamp " << rgba_string(clamp.inside) << " "
                << rgba_string(clamp.beyond) << " | mirror "
                << rgba_string(mirror.inside) << " " << rgba_string(mirror.beyond));
  REQUIRE(repeat.err.empty());
  REQUIRE(clamp.err.empty());
  REQUIRE(mirror.err.empty());

  const auto red = [](auto c) { return c[0] > 150 && c[2] < 60; };
  const auto blue = [](auto c) { return c[2] > 150 && c[0] < 60; };
  CHECK(red(repeat.inside));
  CHECK(red(clamp.inside));
  CHECK(red(mirror.inside));
  CHECK(red(repeat.beyond));
  CHECK(blue(clamp.beyond));
  CHECK(blue(mirror.beyond));
}
