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
// The preset lives in the user library; see GfxMaterialPresetsCompile for how
// it is found. Without it the test skips.
//
// Registration:
//   score_add_gfx_test(material_preset_wrap GfxMaterialPresetWrap.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>
#include <Library/LibrarySettings.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QImage>

#include <algorithm>
#include <array>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

QString presetDir()
{
  const QString env = qEnvironmentVariable("SCORE_CSF_PRESETS");
  if(!env.isEmpty())
    return env;
  return QDir::homePath()
         + QStringLiteral(
             "/Documents/ossia/score/packages/csf-examples/presets/rasterizers");
}

QString libraryRoot()
{
  QDir root(presetDir());
  return root.cd(QStringLiteral("../../../..")) ? root.absolutePath() : QString{};
}

std::shared_ptr<ossia::buffer_resource> cpuBuffer(
    std::vector<float> data, ossia::buffer_data::usage usage)
{
  auto owned = std::make_shared<std::vector<float>>(std::move(data));
  auto res = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(owned, owned->data());
  bd.byte_size = int64_t(owned->size() * sizeof(float));
  bd.usage_hint = usage;
  res->resource = bd;
  res->dirty_index = 1;
  return res;
}

ossia::material_component_ptr splitMaterial(ossia::texture_address_mode wrap)
{
  QImage img(64, 64, QImage::Format_RGBA8888);
  img.fill(QColor(255, 0, 0, 255));
  for(int y = 0; y < img.height(); ++y)
    for(int x = img.width() / 2; x < img.width(); ++x)
      img.setPixelColor(x, y, QColor(0, 0, 255, 255));
  QByteArray png;
  {
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
  }
  auto src = std::make_shared<ossia::texture_source>();
  src->embedded_data = std::make_shared<std::vector<uint8_t>>(
      reinterpret_cast<const uint8_t*>(png.constData()),
      reinterpret_cast<const uint8_t*>(png.constData()) + png.size());
  src->mime_type = "image/png";

  auto m = std::make_shared<ossia::material_component>();
  m->stable_id = 0x3A7E0001u;
  m->unlit = true;
  m->base_color_texture.source = std::move(src);
  m->base_color_texture.sampler.wrap_s = wrap;
  m->base_color_texture.sampler.wrap_t = wrap;
  return m;
}

std::shared_ptr<ossia::scene_state> makeState(ossia::material_component_ptr mat)
{
  constexpr float e = 0.8f;
  auto pos = cpuBuffer(
      {-e, -e, 0, e, -e, 0, e, e, 0, -e, -e, 0, e, e, 0, -e, e, 0},
      ossia::buffer_data::usage::vertex_buffer);
  auto uv = cpuBuffer(
      {0, 0, 2, 0, 2, 1, 0, 0, 2, 1, 0, 1},
      ossia::buffer_data::usage::vertex_buffer);

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

struct SceneNode final : score::gfx::ProcessNode
{
  std::shared_ptr<ossia::scene_state> state;
  SceneNode()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct SceneRenderer final : score::gfx::NodeRenderer
{
  const SceneNode& self;
  ossia::scene_spec m_scene;
  explicit SceneRenderer(const SceneNode& n)
      : NodeRenderer{n}
      , self{n}
  {
  }
  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_initialized = true;
  }
  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
    m_scene.state = self.state;
  }
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(!m_scene.state || !edge.sink || !edge.sink->node)
      return;
    auto rn = edge.sink->node->renderedNodes.find(&renderer);
    if(rn == edge.sink->node->renderedNodes.end())
      return;
    auto& in = edge.sink->node->input;
    auto it = std::find(in.begin(), in.end(), edge.sink);
    if(it != in.end())
      rn->second->process(int(it - in.begin()), m_scene, edge.source);
  }
  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override
  {
    m_scene = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
SceneNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new SceneRenderer{*this};
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
  const QDir dir(presetDir());
  const QString vs = dir.filePath(QStringLiteral("classic_pbr_full.vert"));
  const QString fs = dir.filePath(QStringLiteral("classic_pbr_full.frag"));
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    auto& lib = ctx.settings<Library::Settings::Model>();
    const QString previousRoot = lib.getRootPath();
    lib.setRootPath(libraryRoot());
    struct Restore
    {
      Library::Settings::Model& lib;
      QString root;
      ~Restore() { lib.setRootPath(root); }
    } restore{lib, previousRoot};

    GfxPipeline p;
    auto node = std::make_unique<SceneNode>();
    node->state = makeState(splitMaterial(wrap));
    const int hn = p.addNode(std::move(node));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
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

std::string rgb(std::array<uint8_t, 4> c)
{
  return "(" + std::to_string(c[0]) + "," + std::to_string(c[1]) + ","
         + std::to_string(c[2]) + ")";
}
}

TEST_CASE(
    "classic_pbr_full applies a texture's wrap mode", "[gfx][presets][material][sampler]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!QFileInfo::exists(QDir(presetDir()).filePath("classic_pbr_full.frag")))
    SKIP("preset library not found at " + presetDir().toStdString());

  const auto repeat = render(api, ossia::texture_address_mode::REPEAT);
  if(repeat.skipped)
    SKIP("backend unavailable");
  const auto clamp = render(api, ossia::texture_address_mode::CLAMP_TO_EDGE);
  const auto mirror = render(api, ossia::texture_address_mode::MIRROR);

  INFO("repeat " << rgb(repeat.inside) << " " << rgb(repeat.beyond) << " | clamp "
                 << rgb(clamp.inside) << " " << rgb(clamp.beyond) << " | mirror "
                 << rgb(mirror.inside) << " " << rgb(mirror.beyond));
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
