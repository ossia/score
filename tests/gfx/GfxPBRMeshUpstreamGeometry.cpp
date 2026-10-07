// PBR Mesh keeps what the geometry wired into it carries besides its vertices:
// its auxiliary buffers and textures, and its transform.
//
// The harness node plays the part of the avnd wrapper around Threedim::PBRMesh:
// it fills the node's GPU geometry inlet the way the wrapper does (GPU buffer
// handles, auxiliaries, the upstream transform matrix), runs operator()(),
// init() and update() on the render thread, and publishes the node's scene.
// Downstream is the real ScenePreprocessorNode and a raw raster, read back as
// pixels.
//
// - auxiliaries: the raster declares a pbr_aux_color buffer and a pbr_aux_tex
//   texture, resolved by name from the preprocessor's output geometry. The
//   geometry carries them as auxiliaries; the quad's red comes from the
//   buffer and its blue from the texture.
// - transform: the geometry's transform translates by +0.4 in x and rotates,
//   PBR Mesh scales by 2. PBR Mesh's TRS applies on top of the upstream one,
//   so the quad centred at the origin lands at x = 2 * 0.4 = 0.8 (and not at
//   0.4, the other order, nor at 0, dropping the upstream transform).
// - a geometry coming out of another Scene Preprocessor carries that
//   preprocessor's scene auxiliaries; here an all-red scene_materials. The
//   quad must take PBR Mesh's own green material from the next
//   preprocessor's scene_materials, not the forwarded one.
#include <score_test/Gfx.hpp>

#include "GfxSceneSource.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Threedim/PBRMesh.hpp>

#include <QColor>
#include <QImage>
#include <QMatrix4x4>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstring>
#include <vector>

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

struct PBRMeshHarness final : score::gfx::ProcessNode
{
  float upstream[16]{};
  float scale{1.f};
  // The geometry also carries a scene_materials auxiliary, as the output of
  // another Scene Preprocessor does.
  bool preprocessed{false};
  // Written on the render thread; read after the render on the test thread.
  mutable ossia::scene_spec published;

  PBRMeshHarness()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct PBRMeshHarnessRenderer final : score::test::gfx::scene::SourceRenderer
{
  const PBRMeshHarness& self;
  Threedim::PBRMesh pbr;
  QRhiBuffer* m_pos{};
  QRhiBuffer* m_aux{};
  QRhiTexture* m_tex{};
  QRhiBuffer* m_mats{};

  explicit PBRMeshHarnessRenderer(const PBRMeshHarness& n)
      : SourceRenderer{n}
      , self{n}
  {
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    // A quad of half-size 0.1 centred at the origin, vec4 positions.
    const float h = 0.1f;
    const float quad[24]{-h, -h, 0, 1, h, -h, 0, 1, h, h, 0, 1,
                         -h, -h, 0, 1, h, h,  0, 1, -h, h, 0, 1};
    m_pos = r.state.rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer, sizeof(quad));
    m_pos->create();
    res.uploadStaticBuffer(m_pos, quad);

    const float color[4]{1.f, 0.f, 0.f, 1.f};
    m_aux = r.state.rhi->newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer, sizeof(color));
    m_aux->create();
    res.uploadStaticBuffer(m_aux, color);

    QImage img(4, 4, QImage::Format_RGBA8888);
    img.fill(QColor(0, 0, 192, 255));
    m_tex = r.state.rhi->newTexture(QRhiTexture::RGBA8, QSize(4, 4));
    m_tex->create();
    res.uploadTexture(m_tex, img);

    auto& m = pbr.inputs.geometry_in.mesh;
    m.buffers.push_back({.handle = m_pos, .byte_size = sizeof(quad)});
    m.buffers.push_back({.handle = m_aux, .byte_size = sizeof(color)});
    m.bindings.push_back(
        {.stride = 16,
         .step_rate = 1,
         .classification = halp::binding_classification::per_vertex});
    m.attributes.push_back(
        {.binding = 0,
         .semantic = halp::attribute_semantic::position,
         .format = halp::attribute_format::float4});
    m.input.push_back({.buffer = 0, .byte_offset = 0});
    m.auxiliary.push_back(
        {.name = "pbr_aux_color",
         .buffer = 1,
         .byte_offset = 0,
         .byte_size = sizeof(color)});
    m.auxiliary_textures.push_back({.name = "pbr_aux_tex", .handle = m_tex});
    if(self.preprocessed)
    {
      // 16 red MaterialGPU entries (80 bytes, baseColor first).
      std::vector<float> mats(16 * 20, 0.f);
      for(int i = 0; i < 16; i++)
      {
        mats[i * 20 + 0] = 1.f;
        mats[i * 20 + 3] = 1.f;
      }
      const auto bytes = int64_t(mats.size() * sizeof(float));
      m_mats = r.state.rhi->newBuffer(
          QRhiBuffer::Static, QRhiBuffer::StorageBuffer, bytes);
      m_mats->create();
      res.uploadStaticBuffer(m_mats, mats.data());
      m.buffers.push_back({.handle = m_mats, .byte_size = bytes});
      m.auxiliary.push_back(
          {.name = "scene_materials",
           .buffer = (int)m.buffers.size() - 1,
           .byte_offset = 0,
           .byte_size = bytes});
    }
    m.vertices = 6;
    m.topology = halp::primitive_topology::triangles;
    std::copy_n(self.upstream, 16, pbr.inputs.geometry_in.transform);
    pbr.inputs.scale.value = {self.scale, self.scale, self.scale};
    pbr.inputs.base_color.value = {0.f, 1.f, 0.f, 1.f};

    pbr.init(r, res);
    m_initialized = true;
  }

  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e) override
  {
    pbr();
    pbr.update(r, res, e);
    m_scene = pbr.outputs.scene_out.scene;
    self.published = m_scene;
  }

  void release(score::gfx::RenderList& r) override
  {
    pbr.release(r);
    delete m_pos;
    delete m_aux;
    delete m_tex;
    delete m_mats;
    m_pos = m_aux = m_mats = nullptr;
    m_tex = nullptr;
    SourceRenderer::release(r);
  }
};

score::gfx::NodeRenderer*
PBRMeshHarness::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new PBRMeshHarnessRenderer{*this};
}

struct Result
{
  bool skipped{};
  std::string err;
  ReadbackImage img;
  ossia::scene_spec scene;
};

Result render(
    score::gfx::GraphicsApi api, const char* vs, const char* fs, const float* upstream,
    float scale, bool preprocessed = false)
{
  Result res;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto node = std::make_unique<PBRMeshHarness>();
    std::copy_n(upstream, 16, node->upstream);
    node->scale = scale;
    node->preprocessed = preprocessed;
    auto* harness = node.get();
    const int hn = p.addNode(std::move(node));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(corpus(vs), corpus(fs));
    if(hn < 0 || flat < 0 || raster < 0)
    {
      res.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      res.skipped = p.skipped();
      res.err = res.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    res.img = p.readback(sink);
    if(!res.img.valid())
      res.err = "readback failed";
    res.scene = harness->published;
  });
  return res;
}

constexpr float kIdentity[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

TEST_CASE(
    "the auxiliaries of the geometry wired into PBR Mesh reach a raster after "
    "the Scene Preprocessor",
    "[gfx][scene][pbrmesh]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = render(
      api, "syn-scene-pbrmesh-aux.vs", "syn-scene-pbrmesh-aux.fs", kIdentity, 1.f);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());

  const auto c = r.img.at(kSize / 2, kSize / 2);
  const auto bg = r.img.at(2, 2);
  INFO(
      "centre rgba=" << score::test::gfx::scene::rgba_string(c)
                     << " background rgba=" << score::test::gfx::scene::rgba_string(bg));
  // The quad is drawn (g), r comes from the pbr_aux_color buffer, b from the
  // pbr_aux_tex texture.
  REQUIRE(c[1] > 240);
  CHECK(c[0] > 240);
  CHECK(c[2] > 180);
  CHECK(c[2] < 205);
}

TEST_CASE(
    "PBR Mesh places its mesh with its own TRS on top of the transform of the "
    "geometry wired into it",
    "[gfx][scene][pbrmesh]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // The rotation leaves the square quad's footprint unchanged; the flattened
  // matrix below checks it.
  QMatrix4x4 up;
  up.translate(0.4f, 0.f, 0.f);
  up.rotate(30.f, 0.f, 0.f, 1.f);
  float upstream[16];
  std::memcpy(upstream, up.constData(), sizeof(upstream));

  const auto r = render(
      api, "syn-scene-perdraw-solid.vs", "syn-scene-perdraw-solid.fs", upstream, 2.f);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());

  // The flattened draw's model matrix is scale(2) * upstream.
  score::gfx::FlatScene flat;
  score::gfx::flattenScene(r.scene, flat, 1.f);
  REQUIRE(flat.draws.size() == 1);
  QMatrix4x4 expected;
  expected.scale(2.f);
  expected *= up;
  const float* got = flat.draws[0].worldTransform.constData();
  for(int i = 0; i < 16; i++)
  {
    CAPTURE(i);
    CHECK(got[i] == Catch::Approx(expected.constData()[i]).margin(1e-5));
  }

  // x in NDC: 0 -> column 32, 0.4 -> 44.8, 0.8 -> 57.6. The quad is centred
  // at x = 0.8 and at least 0.2 wide on each side.
  const auto at0 = r.img.at(32, kSize / 2);
  const auto at04 = r.img.at(45, kSize / 2);
  const auto at08 = r.img.at(57, kSize / 2);
  INFO(
      "x=0 " << int(at0[0]) << " x=0.4 " << int(at04[0]) << " x=0.8 "
             << int(at08[0]));
  CHECK(at0[0] < 40);
  CHECK(at04[0] < 40);
  CHECK(at08[0] > 215);
}

TEST_CASE(
    "PBR Mesh fed a geometry that never sent a transform places its mesh with "
    "its own TRS alone",
    "[gfx][scene][pbrmesh]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // The inlet's matrix stays zero-filled until the upstream sends one.
  const float none[16]{};
  const auto r = render(
      api, "syn-scene-perdraw-solid.vs", "syn-scene-perdraw-solid.fs", none, 2.f);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());

  // scale(2): the quad spans [-0.2, 0.2].
  const auto at0 = r.img.at(32, kSize / 2);
  const auto at08 = r.img.at(57, kSize / 2);
  INFO("x=0 " << int(at0[0]) << " x=0.8 " << int(at08[0]));
  CHECK(at0[0] > 215);
  CHECK(at08[0] < 40);
}

TEST_CASE(
    "a geometry coming out of a Scene Preprocessor into PBR Mesh does not "
    "shadow the next preprocessor's scene auxiliaries",
    "[gfx][scene][pbrmesh]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // The upstream scene_materials is all red; PBR Mesh's own material is green.
  const auto r = render(
      api, "syn-scene-pbrmesh-material.vs", "syn-scene-pbrmesh-material.fs",
      kIdentity, 1.f, true);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.err);
  REQUIRE(r.err.empty());

  const auto c = r.img.at(kSize / 2, kSize / 2);
  INFO("centre rgba=" << score::test::gfx::scene::rgba_string(c));
  CHECK(c[0] < 40);
  CHECK(c[1] > 215);
}
