// Library presets render what they claim to, on every backend.
//
// Drives the shipped presets from the user library through the real chain --
// a scene with a lit quad, ScenePreprocessor, the preset -- and reads pixels
// back:
//  * blinn_phong reads scene_lights in the current RawLight layout: a
//    directional light facing the quad brightens it.
//  * Each shading tier (unlit, blinn_phong, pbr_fast, classic_pbr) draws a
//    textured material's own texture with its array input left unwired, and
//    an untextured one in its base colour.
//  * The lit tiers shade a lit quad instead of drawing it black, and a
//    shadow-casting light with no cascades wired leaves it unshadowed.
//  * A normal map, and classic_pbr's clear coat normal map, are scaled by the
//    material's normal scale.
//  * In the Points Mode each tier draws its vertices point_size pixels wide.
//  * classic_pbr_openpbr's default bsdf_intensity_scale, with the eight
//    OpenPBR lookup tables wired, gives a directional light the same direct
//    contribution classic_pbr gives it.
//  * cubemap_orbit.fs shows +Y at the top of the view, as cubemap_view.fs.
//  * The stock compute presets use a workgroup of at most 256 invocations,
//    and Game of Life seeds a board and advances it by exact Conway
//    generations, identically across runs.
//
// The presets come from the library at SCORE_TEST_LIBRARY_ROOT; without it
// the cases skip.
#include "GfxSceneSource.hpp"
#include "GfxUserLibrary.hpp"
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <QDir>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>

#include <algorithm>
#include <array>
#include <cstdint>
#include <numbers>
#include <optional>

#if defined(LIBRARY_PRESETS_OPENPBR_BSDF)
namespace openpbr_luts
{
struct OpenpbrVec3
{
  float x, y, z;
};
using vec3 = OpenpbrVec3;
#define OPENPBR_CONSTEXPR_GLOBAL static inline constexpr
#define OPENPBR_UINT16 std::uint16_t
#define OPENPBR_UINT32 std::uint32_t
#define OPENPBR_ENERGY_TABLES_USE_UINT16 1
// clang-format off
#include <openpbr_settings.h>
#include <openpbr_data_constants.h>
#include <impl/data/openpbr_energy_arrays.h>
#include <impl/data/openpbr_ltc_array.h>
// clang-format on
}
#endif

using namespace score::test::gfx;

namespace
{
constexpr int kSize = 64;

using scene::cpu_buffer;
using scene::rgba_string;
using score::test::gfx::isf::corpus;

const QString kRasterizers = QStringLiteral("presets/rasterizers/");
const QString kComputeShaders = QStringLiteral("Presets/Compute Shader");

QString preset(const char* name)
{
  return library::find(kRasterizers + QLatin1String(name));
}

QString computePreset(const char* name)
{
  return library::find(kComputeShaders + QLatin1Char('/') + QLatin1String(name));
}

std::shared_ptr<ossia::texture_source> solidPng(QColor c)
{
  QImage img(16, 16, QImage::Format_RGBA8888);
  img.fill(c);
  return scene::png_source(img);
}

struct SceneOpts
{
  std::array<float, 4> base{0.5f, 0.5f, 0.5f, 1.f};
  bool redTexture = false;
  bool light = false;
  float intensity = 1.f;
  bool castShadow = false;
  // A normal map tilting the normal 53 degrees off the face, with this scale.
  std::optional<float> normalScale;
  // A clear coat whose normal map tilts the same way, with this scale.
  std::optional<float> coatNormalScale;
  // Drawn in the Points Mode, with this point_size.
  std::optional<float> pointSize;
};

ossia::material_component_ptr makeMaterial(const SceneOpts& o)
{
  auto m = std::make_shared<ossia::material_component>();
  m->stable_id = 0x0F1A0001u;
  std::copy(o.base.begin(), o.base.end(), m->base_color_factor);
  m->metallic_factor = 0.f;
  m->roughness_factor = 1.f;
  if(o.redTexture)
    m->base_color_texture.source = solidPng(QColor(255, 0, 0, 255));
  // Tangent-space (0.8, 0, 0.6).
  const QColor tilted(230, 128, 204, 255);
  if(o.normalScale)
  {
    m->normal_texture.source = solidPng(tilted);
    m->normal_scale = *o.normalScale;
  }
  if(o.coatNormalScale)
  {
    m->clearcoat.factor = 1.f;
    m->clearcoat.roughness_factor = 0.5f;
    m->clearcoat.normal_texture.source = solidPng(tilted);
    m->clearcoat.normal_scale = *o.coatNormalScale;
  }
  return m;
}

std::shared_ptr<ossia::scene_state>
makeState(const SceneOpts& o, ossia::gpu_slot_ref lightRef, ossia::gpu_slot_ref xformRef)
{
  constexpr float e = 0.8f;
  auto pos = cpu_buffer(
      {-e, -e, 0, e, -e, 0, e, e, 0, -e, -e, 0, e, e, 0, -e, e, 0},
      ossia::buffer_data::usage::vertex_buffer);
  auto nrm = cpu_buffer(
      {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
      ossia::buffer_data::usage::vertex_buffer);
  auto uv = cpu_buffer(
      {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1}, ossia::buffer_data::usage::vertex_buffer);

  auto mat = makeMaterial(o);

  ossia::mesh_primitive prim;
  prim.vertex_buffers = {pos, nrm, uv};
  auto attr
      = [&](ossia::attribute_semantic s, ossia::vertex_format f, int buf, int stride) {
    ossia::vertex_attribute a;
    a.semantic = s;
    a.format = f;
    a.buffer_index = buf;
    a.byte_stride = stride;
    prim.attributes.push_back(a);
  };
  attr(ossia::attribute_semantic::position, ossia::vertex_format::float3, 0, 12);
  attr(ossia::attribute_semantic::normal, ossia::vertex_format::float3, 1, 12);
  attr(ossia::attribute_semantic::texcoord0, ossia::vertex_format::float2, 2, 8);
  prim.topology = ossia::primitive_topology::triangles;
  prim.vertex_count = 6;
  prim.stable_id = 0x0F1A0002u;
  prim.bounds = {{-e, -e, 0.f}, {e, e, 0.f}};
  prim.material = mat;

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;
  auto children = std::make_shared<std::vector<ossia::scene_payload>>();
  children->push_back(ossia::mesh_component_ptr(std::move(mesh)));

  if(o.light && lightRef.size != 0)
  {
    auto lc = std::make_shared<ossia::light_component>();
    lc->type = ossia::light_type::directional;
    lc->intensity = o.intensity;
    lc->shadow.enabled = o.castShadow;
    lc->raw_slot = lightRef;
    lc->stable_id = 0x0F1A0003u;
    lc->dirty_index = 1;

    ossia::scene_transform xf;
    xf.raw_slot = xformRef;
    xf.stable_id = 0x0F1A0004u;

    auto lchildren = std::make_shared<std::vector<ossia::scene_payload>>();
    lchildren->push_back(xf);
    lchildren->push_back(ossia::light_component_ptr(std::move(lc)));
    auto lnode = std::make_shared<ossia::scene_node>();
    lnode->children = std::move(lchildren);
    children->push_back(ossia::scene_node_ptr(std::move(lnode)));
  }

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
  SceneOpts opts;
  SceneNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

// Plays the part of the Light process: owns a RawLight and a RawTransform
// arena slot, uploads a directional light pointing down local -Z (towards the
// quad, which faces +Z), and stamps both refs on the scene it emits.
struct SceneRenderer final : scene::SourceRenderer
{
  const SceneNode& self;
  score::gfx::GpuResourceRegistry::Slot m_light, m_xform;

  explicit SceneRenderer(const SceneNode& n)
      : SourceRenderer{n}
      , self{n}
  {
  }
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    ossia::gpu_slot_ref lref, xref;
    if(self.opts.light)
    {
      auto& reg = r.registry();
      m_light = reg.allocate(
          score::gfx::GpuResourceRegistry::Arena::RawLight,
          sizeof(score::gfx::RawLightData));
      m_xform = reg.allocate(
          score::gfx::GpuResourceRegistry::Arena::RawTransform,
          sizeof(score::gfx::RawLocalTransform));
      lref = reg.toOssiaRef(m_light);
      xref = reg.toOssiaRef(m_xform);
      upload(r, res);
    }
    m_scene.state = makeState(self.opts, lref, xref);
    m_initialized = true;
  }
  void upload(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res)
  {
    if(!m_light.valid() || !m_xform.valid())
      return;
    score::gfx::RawLightData raw{};
    raw.color[3] = self.opts.intensity;
    raw.local_direction[3] = 0.f;
    raw.shadow_enabled = self.opts.castShadow ? 1u : 0u;
    raw.transform_slot = m_xform.slot_index;
    r.registry().updateSlot(res, m_light, &raw, sizeof(raw));
    score::gfx::RawLocalTransform xf{};
    r.registry().updateSlot(res, m_xform, &xf, sizeof(xf));
  }
  void update(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*)
      override
  {
    upload(r, res);
  }
  void release(score::gfx::RenderList& r) override
  {
    if(m_light.valid())
      r.registry().free(m_light);
    if(m_xform.valid())
      r.registry().free(m_xform);
    SourceRenderer::release(r);
  }
};

score::gfx::NodeRenderer*
SceneNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new SceneRenderer{*this};
}

// A node's image inlet for a named INPUT: image-like inputs (and a CSF's
// read-only images) create the node's Image ports in declaration order.
score::gfx::Port* imageInputByName(score::gfx::ISFNode& n, std::string_view name)
{
  std::vector<score::gfx::Port*> images;
  for(auto* p : n.input)
    if(p->type == score::gfx::Types::Image)
      images.push_back(p);
  std::size_t k = 0;
  for(const auto& in : n.descriptor().inputs)
  {
    const auto* csf = ossia::get_if<::isf::csf_image_input>(&in.data);
    const bool image = ossia::get_if<::isf::image_input>(&in.data)
                       || ossia::get_if<::isf::cubemap_input>(&in.data)
                       || ossia::get_if<::isf::texture_input>(&in.data)
                       || (csf && csf->access == "read_only");
    if(!image)
      continue;
    if(in.name == name)
      return k < images.size() ? images[k] : nullptr;
    ++k;
  }
  return nullptr;
}

// A float control by name: float INPUTs create the node's Float ports in
// declaration order.
void setFloat(score::gfx::ISFNode& n, std::string_view name, float v)
{
  std::vector<int> floatPorts;
  for(int i = 0; i < int(n.input.size()); ++i)
    if(n.input[i]->type == score::gfx::Types::Float)
      floatPorts.push_back(i);
  std::size_t k = 0;
  for(const auto& in : n.descriptor().inputs)
  {
    if(!ossia::get_if<::isf::float_input>(&in.data))
      continue;
    if(in.name == name && k < floatPorts.size())
      setControl(n, floatPorts[k], ossia::value{v});
    ++k;
  }
}

#if defined(LIBRARY_PRESETS_OPENPBR_BSDF)
// The eight OpenPBR lookup tables, uploaded the way Academy's "OpenPBR LUTs"
// process does, in OpenPBR_LutId order. Without them the preset's energy
// compensation reads zero and its diffuse lobe vanishes.
struct OpenpbrLutNode final : score::gfx::ProcessNode
{
  OpenpbrLutNode()
  {
    for(int i = 0; i < 8; ++i)
      output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct OpenpbrLutRenderer final : score::gfx::NodeRenderer
{
  const OpenpbrLutNode& self;
  std::array<QRhiTexture*, 8> tex{};
  explicit OpenpbrLutRenderer(const OpenpbrLutNode& n)
      : NodeRenderer{n}
      , self{n}
  {
  }
  static QRhiTexture* table2D(
      QRhi& rhi, QRhiResourceUpdateBatch& res, const std::uint16_t* src, int w, int h)
  {
    auto* t = rhi.newTexture(QRhiTexture::R16, QSize(w, h), 1, {});
    t->create();
    QRhiTextureSubresourceUploadDescription sub{
        src, quint32(std::size_t(w) * h * sizeof(std::uint16_t))};
    res.uploadTexture(t, QRhiTextureUploadDescription{{0, 0, sub}});
    return t;
  }
  static QRhiTexture*
  table3D(QRhi& rhi, QRhiResourceUpdateBatch& res, const std::uint16_t* src, int n)
  {
    auto* t
        = rhi.newTexture(QRhiTexture::R16, n, n, n, 1, QRhiTexture::ThreeDimensional);
    t->create();
    const std::size_t slice = std::size_t(n) * n * sizeof(std::uint16_t);
    std::vector<QRhiTextureUploadEntry> entries;
    for(int z = 0; z < n; ++z)
      entries.push_back(
          QRhiTextureUploadEntry{
              z, 0,
              QRhiTextureSubresourceUploadDescription{
                  reinterpret_cast<const char*>(src) + z * slice, quint32(slice)}});
    QRhiTextureUploadDescription desc;
    desc.setEntries(entries.begin(), entries.end());
    res.uploadTexture(t, desc);
    return t;
  }
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    using namespace openpbr_luts;
    auto& rhi = *r.state.rhi;
    constexpr int N = OpenPBR_EnergyTableSize;
    tex[OpenPBR_LutId_IdealDielectricEnergyComplement]
        = table3D(rhi, res, OpenPBR_IdealDielectricEnergyComplement_Array, N);
    tex[OpenPBR_LutId_IdealDielectricAverageEnergyComplement]
        = table2D(rhi, res, OpenPBR_IdealDielectricAverageEnergyComplement_Array, N, N);
    tex[OpenPBR_LutId_IdealDielectricReflectionRatio]
        = table2D(rhi, res, OpenPBR_IdealDielectricReflectionRatio_Array, N, N);
    tex[OpenPBR_LutId_OpaqueDielectricEnergyComplement]
        = table3D(rhi, res, OpenPBR_OpaqueDielectricEnergyComplement_Array, N);
    tex[OpenPBR_LutId_OpaqueDielectricAverageEnergyComplement]
        = table2D(rhi, res, OpenPBR_OpaqueDielectricAverageEnergyComplement_Array, N, N);
    tex[OpenPBR_LutId_IdealMetalEnergyComplement]
        = table2D(rhi, res, OpenPBR_IdealMetalEnergyComplement_Array, N, N);
    tex[OpenPBR_LutId_IdealMetalAverageEnergyComplement]
        = table2D(rhi, res, OpenPBR_IdealMetalAverageEnergyComplement_Array, N, 1);
    {
      constexpr int L = OpenPBR_LTCTableSize;
      auto* t = rhi.newTexture(QRhiTexture::RGBA32F, QSize(L, L), 1, {});
      t->create();
      std::vector<float> rgba(std::size_t(L) * L * 4);
      for(int i = 0; i < L * L; ++i)
      {
        rgba[i * 4 + 0] = OpenPBR_LTC_Array[i].x;
        rgba[i * 4 + 1] = OpenPBR_LTC_Array[i].y;
        rgba[i * 4 + 2] = OpenPBR_LTC_Array[i].z;
      }
      QRhiTextureSubresourceUploadDescription sub{
          rgba.data(), quint32(rgba.size() * sizeof(float))};
      res.uploadTexture(t, QRhiTextureUploadDescription{{0, 0, sub}});
      tex[OpenPBR_LutId_LTC] = t;
    }
    m_initialized = true;
  }
  void
  update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
  }
  QRhiTexture* textureForOutput(const score::gfx::Port& output) override
  {
    for(int i = 0; i < 8; ++i)
      if(self.output[i] == &output)
        return tex[i];
    return nullptr;
  }
  void
  runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override
  {
    for(auto*& t : tex)
    {
      delete t;
      t = nullptr;
    }
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
OpenpbrLutNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new OpenpbrLutRenderer{*this};
}

constexpr std::array<const char*, 8> kLutInputs{
    "openpbr_lut_ideal_dielectric_3d",
    "openpbr_lut_ideal_dielectric_avg",
    "openpbr_lut_dielectric_ratio",
    "openpbr_lut_opaque_dielectric_3d",
    "openpbr_lut_opaque_dielectric_avg",
    "openpbr_lut_metal",
    "openpbr_lut_metal_avg",
    "openpbr_lut_ltc"};
#endif

// A node built outside the app starts a float control whose DEFAULT is 0 at
// the middle of its range; the app sends the declared default. Send it here
// too, so each preset runs with the values it declares.
void applyZeroDefaults(score::gfx::ISFNode& n)
{
  std::vector<int> floatPorts;
  for(int i = 0; i < int(n.input.size()); ++i)
    if(n.input[i]->type == score::gfx::Types::Float)
      floatPorts.push_back(i);
  std::size_t k = 0;
  for(const auto& in : n.descriptor().inputs)
  {
    if(auto* f = ossia::get_if<::isf::float_input>(&in.data))
    {
      if(k < floatPorts.size() && f->def == 0.)
        setControl(n, floatPorts[k], ossia::value{0.f});
      ++k;
    }
  }
}

struct Probe
{
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> center{};
  int lit = 0; // pixels brighter than 8 in any colour channel
};

Probe renderScene(
    score::gfx::GraphicsApi api, const char* vs, const char* fs, SceneOpts opts,
    bool openpbrLuts = false)
{
  Probe out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    library::RootGuard root{ctx};
    GfxPipeline p;
    auto node = std::make_unique<SceneNode>();
    node->opts = opts;
    const int hn = p.addNode(std::move(node));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(preset(vs), preset(fs));
    if(hn < 0 || flat < 0 || raster < 0)
    {
      out.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.nodeSceneOut(hn, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
#if defined(LIBRARY_PRESETS_OPENPBR_BSDF)
    if(openpbrLuts)
    {
      const int luts = p.addNode(std::make_unique<OpenpbrLutNode>());
      for(int i = 0; i < 8; ++i)
      {
        auto* in = imageInputByName(*p.isf(raster), kLutInputs[i]);
        if(!in)
        {
          out.err = std::string("no inlet ") + kLutInputs[i];
          return;
        }
        p.wire(p.nodeImageOut(luts, i), in);
      }
    }
#endif
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.err = out.skipped ? std::string{} : p.error();
      return;
    }
    applyZeroDefaults(*p.isf(raster));
    if(opts.pointSize)
    {
      constexpr int modePoints = 1;
      setControl(
          *p.isf(raster), nth_control_input(*p.isf(raster), 0),
          ossia::value{modePoints});
      setFloat(*p.isf(raster), "point_size", *opts.pointSize);
    }
    p.render(5);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      out.err = "readback failed";
      return;
    }
    out.center = img.at(kSize / 2, kSize / 2);
    for(int y = 0; y < img.height; ++y)
      for(int x = 0; x < img.width; ++x)
      {
        const auto c = img.at(x, y);
        if(std::max({c[0], c[1], c[2]}) > 8)
          ++out.lit;
      }
  });
  return out;
}

bool haveRasterPresets()
{
  return !preset("classic_pbr.frag").isEmpty();
}

std::string noRasterPresets()
{
  return library::skip_reason(kRasterizers + QStringLiteral("classic_pbr.frag"));
}

Probe renderTier(
    score::gfx::GraphicsApi api, const std::string& tier, SceneOpts opts,
    bool openpbrLuts = false)
{
  return renderScene(
      api, (tier + ".vert").c_str(), (tier + ".frag").c_str(), opts, openpbrLuts);
}
}

TEST_CASE("blinn_phong is lit by a scene light", "[gfx][presets]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!haveRasterPresets())
    SKIP(noRasterPresets());

  SceneOpts dark;
  SceneOpts lit;
  lit.light = true;
  const auto a = renderScene(api, "blinn_phong.vert", "blinn_phong.frag", dark);
  if(a.skipped)
    SKIP("backend unavailable");
  const auto b = renderScene(api, "blinn_phong.vert", "blinn_phong.frag", lit);
  INFO("unlit " << rgba_string(a.center) << " lit " << rgba_string(b.center));
  REQUIRE(a.err.empty());
  REQUIRE(b.err.empty());
  // No environment, so no ambient without a light. With one, the normalised
  // Lambert lobe: 0.5 base colour / pi x N.L = 1, plus a negligible specular.
  CHECK(a.center[0] < 8);
  CHECK(std::abs(int(b.center[0]) - int(255 * 0.5 / std::numbers::pi)) <= 8);
}

TEST_CASE(
    "Each shading tier draws the material's own colour without a wired array",
    "[gfx][presets]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const std::string tier
      = GENERATE(as<std::string>{}, "unlit", "blinn_phong", "pbr_fast", "classic_pbr");
  CAPTURE(backend_name(api), tier);
  if(!haveRasterPresets())
    SKIP(noRasterPresets());

  SceneOpts textured;
  textured.base = {1.f, 1.f, 1.f, 1.f};
  textured.redTexture = true;
  textured.light = true;
  textured.intensity = 3.f;
  SceneOpts plain = textured;
  plain.base = {0.f, 1.f, 0.f, 1.f};
  plain.redTexture = false;
  const auto t = renderTier(api, tier, textured);
  if(t.skipped)
    SKIP("backend unavailable");
  const auto g = renderTier(api, tier, plain);
  INFO("textured " << rgba_string(t.center) << " plain " << rgba_string(g.center));
  REQUIRE(t.err.empty());
  REQUIRE(g.err.empty());
  // The lit tiers scale the base colour by their diffuse lobe (the scene has
  // no environment, hence no ambient term); unlit ignores the light. Only the
  // hue is asserted, and full coverage.
  CHECK(t.center[0] > 8);
  CHECK(t.center[0] > 3 * std::max<int>(t.center[1], t.center[2]));
  CHECK(t.center[3] == 255);
  CHECK(g.center[1] > 8);
  CHECK(g.center[1] > 3 * std::max<int>(g.center[0], g.center[2]));
  CHECK(g.center[3] == 255);
}

TEST_CASE(
    "The lit tiers shade a lit quad, unshadowed without cascades", "[gfx][presets]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const std::string tier
      = GENERATE(as<std::string>{}, "blinn_phong", "pbr_fast", "classic_pbr");
  CAPTURE(backend_name(api), tier);
  if(!haveRasterPresets())
    SKIP(noRasterPresets());

  SceneOpts lit;
  lit.light = true;
  lit.intensity = 3.f;
  const auto s = renderTier(api, tier, lit);
  if(s.skipped)
    SKIP("backend unavailable");
  const auto f = renderTier(api, "classic_pbr", lit);
  SceneOpts casting = lit;
  casting.castShadow = true;
  const auto c = renderTier(api, tier, casting);
  INFO(
      "lit " << rgba_string(s.center) << " casting, no cascades "
             << rgba_string(c.center) << " classic_pbr " << rgba_string(f.center));
  REQUIRE(s.err.empty());
  REQUIRE(c.err.empty());
  REQUIRE(f.err.empty());
  CHECK(s.center[0] > 100);
  // Same direct lighting, different BRDFs and ambient terms: the tiers agree
  // with classic_pbr to a quarter of the range, not exactly.
  CHECK(std::abs(int(s.center[0]) - int(f.center[0])) < 64);
  // A shadow-casting light with no cascades wired is unshadowed.
  CHECK(std::abs(int(c.center[0]) - int(s.center[0])) <= 2);
}

TEST_CASE(
    "The lit tiers scale a normal map by the material's normal scale",
    "[gfx][presets][material]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const std::string tier
      = GENERATE(as<std::string>{}, "blinn_phong", "pbr_fast", "classic_pbr");
  CAPTURE(backend_name(api), tier);
  if(!haveRasterPresets())
    SKIP(noRasterPresets());

  SceneOpts flat;
  flat.light = true;
  flat.intensity = 3.f;
  SceneOpts full = flat;
  full.normalScale = 1.f;
  SceneOpts none = flat;
  none.normalScale = 0.f;
  const auto f = renderTier(api, tier, flat);
  if(f.skipped)
    SKIP("backend unavailable");
  const auto a = renderTier(api, tier, full);
  const auto z = renderTier(api, tier, none);
  INFO(
      "no map " << rgba_string(f.center) << " scale 1 " << rgba_string(a.center)
                << " scale 0 " << rgba_string(z.center));
  REQUIRE(f.err.empty());
  REQUIRE(a.err.empty());
  REQUIRE(z.err.empty());
  // The light faces the quad: the tilted normal takes N.L from 1 to 0.6, and
  // a scale of 0 flattens it back to the face normal.
  CHECK(int(f.center[0]) - int(a.center[0]) > 20);
  CHECK(std::abs(int(z.center[0]) - int(f.center[0])) <= 2);
}

TEST_CASE(
    "classic_pbr scales the clear coat normal map by its normal scale",
    "[gfx][presets][material]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!haveRasterPresets())
    SKIP(noRasterPresets());

  SceneOpts full;
  full.light = true;
  full.intensity = 3.f;
  full.coatNormalScale = 1.f;
  SceneOpts none = full;
  none.coatNormalScale = 0.f;
  const auto a = renderTier(api, "classic_pbr", full);
  if(a.skipped)
    SKIP("backend unavailable");
  const auto z = renderTier(api, "classic_pbr", none);
  INFO("scale 1 " << rgba_string(a.center) << " scale 0 " << rgba_string(z.center));
  REQUIRE(a.err.empty());
  REQUIRE(z.err.empty());
  // Light and view face the quad: the coat's highlight is at its peak with
  // the coat normal on the face normal, and far off it 53 degrees away.
  CHECK(int(z.center[0]) - int(a.center[0]) > 10);
}

TEST_CASE(
    "Each shading tier draws points of its point_size in the Points Mode",
    "[gfx][presets][points]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const std::string tier
      = GENERATE(as<std::string>{}, "unlit", "blinn_phong", "pbr_fast", "classic_pbr");
  CAPTURE(backend_name(api), tier);
  if(!haveRasterPresets())
    SKIP(noRasterPresets());

  SceneOpts one;
  one.light = true;
  one.intensity = 3.f;
  one.pointSize = 1.f;
  SceneOpts six = one;
  six.pointSize = 6.f;
  const auto a = renderTier(api, tier, one);
  if(a.skipped)
    SKIP("backend unavailable");
  const auto b = renderTier(api, tier, six);
  INFO("lit pixels: size 1 " << a.lit << ", size 6 " << b.lit);
  REQUIRE(a.err.empty());
  REQUIRE(b.err.empty());
  // The quad's six vertices land on its four corners, inside the frame.
  CHECK(a.lit >= 1);
  CHECK(a.lit <= 4);
  CHECK(b.lit >= 4 * 30);
  CHECK(b.lit <= 4 * 36);
}

TEST_CASE(
    "classic_pbr_openpbr's default light scale matches classic_pbr",
    "[gfx][presets]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!haveRasterPresets())
    SKIP(noRasterPresets());

#if !defined(LIBRARY_PRESETS_OPENPBR_BSDF)
  SKIP("the OpenPBR-BSDF tables (score-addon-academy) are not available");
#else
  // classic_pbr_openpbr binds more storage buffers than the GL backend offers
  // a fragment stage (binding 31 is refused), independently of this.
  if(api == score::gfx::OpenGL)
    SKIP("classic_pbr_openpbr does not build on OpenGL");
  SceneOpts dark;
  SceneOpts lit;
  lit.light = true;
  const auto o0 = renderScene(
      api, "classic_pbr_openpbr.vert", "classic_pbr_openpbr.frag", dark, true);
  if(o0.skipped)
    SKIP("backend unavailable");
  const auto o1 = renderScene(
      api, "classic_pbr_openpbr.vert", "classic_pbr_openpbr.frag", lit, true);
  const auto f0 = renderTier(api, "classic_pbr", dark);
  const auto f1 = renderTier(api, "classic_pbr", lit);
  INFO(
      "openpbr " << rgba_string(o0.center) << " -> " << rgba_string(o1.center)
                 << " | full " << rgba_string(f0.center) << " -> "
                 << rgba_string(f1.center));
  REQUIRE(o0.err.empty());
  REQUIRE(o1.err.empty());
  REQUIRE(f0.err.empty());
  REQUIRE(f1.err.empty());

  const int dOpen = int(o1.center[0]) - int(o0.center[0]);
  const int dFull = int(f1.center[0]) - int(f0.center[0]);
  // The light's own contribution on a 0.5 grey diffuse quad is ~0.15 through
  // classic_pbr. OpenPBR's energy-conserving diffuse differs from the
  // Lambert lobe by a few percent; the bounds catch a scale that is off by a
  // constant factor, as a wrong default bsdf_intensity_scale would be.
  CHECK(dFull > 20);
  CHECK(dOpen > 0.75 * dFull);
  CHECK(dOpen < 1.33 * dFull);
#endif
}

namespace
{
struct ViewProbe
{
  bool skipped = false;
  std::string err;
  ReadbackImage img;
};

// A corpus CSF feeding one image into a preset viewer, read back at 64x64.
ViewProbe renderViewer(
    score::gfx::GraphicsApi api, const char* producer, const QString& viewerVs,
    const QString& viewer, const char* inlet,
    std::vector<std::pair<const char*, float>> floats)
{
  ViewProbe out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      out.skipped = true;
      out.err = why;
      return;
    }
    GfxPipeline p;
    const int src = p.addCsf(corpus(producer));
    const int view = !viewerVs.isEmpty() ? p.addRaster(viewerVs, viewer)
                     : viewer.endsWith(QStringLiteral(".csf")) ? p.addCsf(viewer)
                                                               : p.addIsf(viewer);
    if(src < 0 || view < 0)
    {
      out.err = "chain build failed: " + p.error();
      return;
    }
    auto* in = imageInputByName(*p.isf(view), inlet);
    if(!in)
    {
      out.err = std::string("no inlet ") + inlet;
      return;
    }
    p.wire(p.imageOut(src, 0), in);
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.err = out.skipped ? std::string{} : p.error();
      return;
    }
    applyZeroDefaults(*p.isf(view));
    for(auto& [name, v] : floats)
      setFloat(*p.isf(view), name, v);
    p.render(3);
    out.img = p.readback(sink);
    if(!out.img.valid())
      out.err = "readback failed";
  });
  return out;
}
}

TEST_CASE(
    "cubemap_orbit.fs shows +Y at the top like cubemap_view.fs",
    "[gfx][presets][cubemap]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const QString orbitFs = preset("cubemap_orbit.fs");
  const QString viewFs = preset("cubemap_view.fs");
  if(orbitFs.isEmpty() || viewFs.isEmpty())
    SKIP(library::skip_reason(kRasterizers + QStringLiteral("cubemap_orbit.fs")));
  // The corpus cube writer paints +Y (0.25, 0.85, 0.25) and -Y
  // (0.85, 0.85, 0.25): red tells the two poles apart. At a 140 degree fov
  // the top and bottom rows look past the horizon into the poles.
  const auto orbit = renderViewer(
      api, "csf-cube-image-write.cs", {}, orbitFs, "skybox", {{"fov", 140.f}});
  if(orbit.skipped)
    SKIP("backend unavailable: " + orbit.err);
  const auto view
      = renderViewer(api, "csf-cube-image-write.cs", {}, viewFs, "skybox", {});
  REQUIRE(orbit.err.empty());
  REQUIRE(view.err.empty());

  const auto oTop = orbit.img.at(kSize / 2, 1);
  const auto oBot = orbit.img.at(kSize / 2, kSize - 2);
  const auto vTop = view.img.at(kSize / 2, 1);
  const auto vBot = view.img.at(kSize / 2, kSize - 2);
  INFO(
      "orbit top " << rgba_string(oTop) << " bottom " << rgba_string(oBot)
                   << " | view top " << rgba_string(vTop) << " bottom "
                   << rgba_string(vBot));
  const auto plusY = [](auto c) { return c[1] > 180 && c[0] < 110; };
  const auto minusY = [](auto c) { return c[1] > 180 && c[0] > 180; };
  CHECK(plusY(vTop));
  CHECK(minusY(vBot));
  CHECK(plusY(oTop));
  CHECK(minusY(oBot));
}

TEST_CASE(
    "the stock compute presets fit a 256-invocation workgroup",
    "[gfx][presets][compute]")
{
  const QDir dir(library::find(kComputeShaders));
  const auto files = dir.entryList({QStringLiteral("*.cs")}, QDir::Files);
  if(library::find(kComputeShaders).isEmpty() || files.isEmpty())
    SKIP(library::skip_reason(kComputeShaders));
  struct Sizes
  {
    std::string file, err;
    std::vector<std::array<int, 3>> local;
  };
  std::vector<Sizes> all;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    for(const auto& f : files)
    {
      Sizes s{f.toStdString(), {}, {}};
      const auto built = make_csf_node(dir.filePath(f));
      if(!built.node)
        s.err = built.error.empty() ? "no node" : built.error;
      else
        for(const auto& pass : built.node->descriptor().csf_passes)
          s.local.push_back(pass.local_size);
      all.push_back(std::move(s));
    }
  });
  REQUIRE(all.size() == std::size_t(files.size()));
  for(const auto& s : all)
  {
    CAPTURE(s.file);
    INFO(s.err);
    REQUIRE(s.err.empty());
    REQUIRE(!s.local.empty());
    for(std::size_t i = 0; i < s.local.size(); ++i)
    {
      CAPTURE(i);
      const auto& ls = s.local[i];
      CHECK(ls[0] * ls[1] * ls[2] <= 256);
    }
  }
}

namespace
{
struct LifeRun
{
  bool skipped = false;
  std::string err;
  std::vector<ReadbackImage> frames;
};

// Frames are pumped by hand so TIME advances one tenth of a second per frame,
// half-way between generation boundaries, at the default Speed of 10
// generations per second. The 256x256 board is drawn
// at Cell Size 4 into a 1024x1024 image, read back at 256x256: one texel per
// cell.
LifeRun runLife(score::gfx::GraphicsApi api, int frames)
{
  LifeRun out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      out.skipped = true;
      out.err = why;
      return;
    }
    GfxPipeline p;
    const int life = p.addCsf(computePreset("Game of Life.cs"));
    if(life < 0)
    {
      out.err = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({256, 256});
    p.wire(p.imageOut(life, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.err = out.skipped ? std::string{} : p.error();
      return;
    }
    for(int f = 0; f < frames; ++f)
    {
      score::gfx::Timings tk{};
      tk.date = ossia::time_value{
          int64_t((f + 0.5) * (ossia::flicks_per_second<double> / 10.))};
      tk.parent_duration
          = ossia::time_value{int64_t(1000 * ossia::flicks_per_second<double>)};
      score::gfx::Message m;
      m.node_id = p.isf(life)->nodeId;
      m.token = tk;
      p.isf(life)->process(std::move(m));
      p.sink(sink)->render();
      out.frames.push_back(p.readback(sink));
    }
  });
  return out;
}

using Board = std::vector<uint8_t>;

Board board(const ReadbackImage& img)
{
  Board b(256 * 256);
  for(int y = 0; y < 256; ++y)
    for(int x = 0; x < 256; ++x)
      b[y * 256 + x] = img.at(x, y)[0] > 127;
  return b;
}

int alive(const Board& b)
{
  return int(std::count(b.begin(), b.end(), uint8_t(1)));
}

// Conway B3/S23 on the 256x256 torus.
Board conway(const Board& b)
{
  Board n(b.size());
  for(int y = 0; y < 256; ++y)
    for(int x = 0; x < 256; ++x)
    {
      int c = 0;
      for(int dy = -1; dy <= 1; ++dy)
        for(int dx = -1; dx <= 1; ++dx)
          if(dx || dy)
            c += b[((y + dy + 256) % 256) * 256 + (x + dx + 256) % 256];
      const bool live = b[y * 256 + x];
      n[y * 256 + x] = live ? (c == 2 || c == 3) : (c == 3);
    }
  return n;
}
}

TEST_CASE(
    "the stock Game of Life preset seeds and steps deterministically",
    "[gfx][presets][compute]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(computePreset("Game of Life.cs").isEmpty())
    SKIP(library::skip_reason(kComputeShaders + QStringLiteral("/Game of Life.cs")));
  constexpr int kFrames = 8;
  const auto a = runLife(api, kFrames);
  if(a.skipped)
    SKIP("backend unavailable: " + a.err);
  const auto b = runLife(api, kFrames);
  REQUIRE(a.err.empty());
  REQUIRE(b.err.empty());
  REQUIRE(a.frames.size() == kFrames);
  REQUIRE(b.frames.size() == kFrames);
  for(auto& f : a.frames)
    REQUIRE(f.valid());
  for(auto& f : b.frames)
    REQUIRE(f.valid());

  std::vector<Board> boards;
  std::string counts;
  for(auto& f : a.frames)
  {
    boards.push_back(board(f));
    counts += std::to_string(alive(boards.back())) + " ";
  }
  INFO("alive per frame " << counts);
  // Random seed at density 0.3 over 65536 cells.
  const int seeded = alive(boards.front());
  CHECK(seeded > 15000);
  CHECK(seeded < 25000);
  // Every change of the board is exactly one Conway generation.
  int steps = 0;
  for(std::size_t i = 0; i + 1 < boards.size(); ++i)
  {
    if(boards[i + 1] == boards[i])
      continue;
    CAPTURE(i);
    CHECK(boards[i + 1] == conway(boards[i]));
    ++steps;
  }
  CHECK(steps >= 2);
  // Two runs from the same seed agree on every generation.
  for(int i = 0; i < kFrames; ++i)
  {
    CAPTURE(i);
    CHECK(board(a.frames[i]) == boards[i]);
    CHECK(board(b.frames[i]) == boards[i]);
  }
}

TEST_CASE("cubemap_array_orbit.frag shows +Y at the top", "[gfx][presets][cubemap]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(preset("cubemap_array_orbit.frag").isEmpty())
    SKIP(
        library::skip_reason(kRasterizers + QStringLiteral("cubemap_array_orbit.frag")));

  // At a 140 degree fov the top and bottom rows look past the horizon into
  // the poles; the faces are flat tints, so only the direction matters.
  const auto r = renderViewer(
      api, "preset-cube-array-faces.cs", preset("cubemap_array_orbit.vert"),
      preset("cubemap_array_orbit.frag"), "cube_array",
      {{"fov", 140.f}, {"gamma", 1.f}});
  if(r.skipped)
    SKIP("backend unavailable: " + r.err);
  INFO(r.err);
  REQUIRE(r.err.empty());
  const auto top = r.img.at(kSize / 2, 1);
  const auto bottom = r.img.at(kSize / 2, kSize - 2);
  INFO("top " << rgba_string(top) << " bottom " << rgba_string(bottom));
  CHECK((top[1] > 180 && top[0] < 110));
  CHECK((bottom[1] > 180 && bottom[0] > 180));
}

TEST_CASE(
    "probe_voxel_orbit_view.csf shows +Y at the top and +X on the right",
    "[gfx][presets][voxel]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const QString viewerRel
      = QStringLiteral("presets/lighting/probe_voxel_orbit_view.csf");
  const QString viewer = library::find(viewerRel);
  if(viewer.isEmpty())
    SKIP(library::skip_reason(viewerRel));

  // Yaw pi/2, pitch 0: the camera sits on +Z looking down -Z, so +X is on the
  // right. The grid is red above y = 0.15, green at x > 0.15 below it and
  // blue at x < -0.15 below it.
  const auto r = renderViewer(
      api, "preset-voxel-markers.cs", {}, viewer, "voxel_grid",
      {{"yaw", 1.5707963f}, {"pitch", 0.f}});
  if(r.skipped)
    SKIP("backend unavailable: " + r.err);
  INFO(r.err);
  REQUIRE(r.err.empty());
  const auto top = r.img.at(kSize / 2, 24);
  const auto right = r.img.at(37, kSize / 2);
  const auto left = r.img.at(26, kSize / 2);
  const auto bottom = r.img.at(kSize / 2, 40);
  INFO(
      "top " << rgba_string(top) << " right " << rgba_string(right) << " left "
             << rgba_string(left) << " bottom " << rgba_string(bottom));
  const auto dominant = [](auto c, int ch) {
    return c[ch] > 60 && c[ch] > 2 * c[(ch + 1) % 3] && c[ch] > 2 * c[(ch + 2) % 3];
  };
  CHECK(dominant(top, 0));
  CHECK(dominant(right, 1));
  CHECK(dominant(left, 2));
  CHECK(!dominant(bottom, 0));
}
