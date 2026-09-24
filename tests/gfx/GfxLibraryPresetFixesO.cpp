// Library presets render what they claim to, on every backend.
//
// Drives the shipped presets from the user library through the real chain --
// a scene with a lit quad, ScenePreprocessor, the preset -- and reads pixels
// back:
//  * blinn_phong reads scene_lights in the current RawLight layout: a
//    directional light facing the quad brightens it.
//  * classic_pbr_textured draws a textured material's own texture with its
//    array input left unwired, and an untextured one in its base colour.
//  * classic_pbr_shadowed shades a lit quad instead of drawing it black.
//  * classic_pbr_openpbr's default bsdf_intensity_scale, with the eight
//    OpenPBR lookup tables wired, gives a directional light the same direct
//    contribution classic_pbr_full gives it.
//  * cubemap_orbit.fs shows +Y at the top of the view, as cubemap_view.fs.
//  * The stock compute presets use a workgroup of at most 256 invocations,
//    and Game of Life seeds a board and advances it by exact Conway
//    generations, identically across runs.
//
// The rasterizer presets come from SCORE_CSF_PRESETS, else the default
// library location; the compute presets from SCORE_COMPUTE_PRESETS, else the
// user library's Presets/Compute Shader. Without them the cases skip.
//
// Registration:
//   score_add_gfx_test(library_preset_fixes_o GfxLibraryPresetFixesO.cpp)
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>
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
#include <cmath>
#include <cstdint>

#if defined(FIXO_OPENPBR_BSDF)
namespace fixo_openpbr
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
#include <openpbr_settings.h>
#include <openpbr_data_constants.h>
#include <impl/data/openpbr_energy_arrays.h>
#include <impl/data/openpbr_ltc_array.h>
}
#endif

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

QString computePresetDir()
{
  const QString env = qEnvironmentVariable("SCORE_COMPUTE_PRESETS");
  if(!env.isEmpty())
    return env;
  return QDir::homePath()
         + QStringLiteral(
             "/Documents/ossia/score/packages/default/Presets/Compute Shader");
}

const bool g_noLibraryScan = qputenv("SCORE_DISABLE_LIBRARY", "1");

QString libraryRoot()
{
  QDir root(presetDir());
  return root.cd(QStringLiteral("../../../..")) ? root.absolutePath() : QString{};
}

QString preset(const char* name)
{
  return QDir(presetDir()).filePath(QString::fromLatin1(name));
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

std::shared_ptr<ossia::texture_source> solidPng(QColor c)
{
  QImage img(16, 16, QImage::Format_RGBA8888);
  img.fill(c);
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
  return src;
}

struct SceneOpts
{
  std::array<float, 4> base{0.5f, 0.5f, 0.5f, 1.f};
  bool redTexture = false;
  bool light = false;
  float intensity = 1.f;
  bool castShadow = false;
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
  return m;
}

std::shared_ptr<ossia::scene_state> makeState(
    const SceneOpts& o, ossia::gpu_slot_ref lightRef, ossia::gpu_slot_ref xformRef)
{
  constexpr float e = 0.8f;
  auto pos = cpuBuffer(
      {-e, -e, 0, e, -e, 0, e, e, 0, -e, -e, 0, e, e, 0, -e, e, 0},
      ossia::buffer_data::usage::vertex_buffer);
  auto nrm = cpuBuffer(
      {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1},
      ossia::buffer_data::usage::vertex_buffer);
  auto uv = cpuBuffer(
      {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1},
      ossia::buffer_data::usage::vertex_buffer);

  auto mat = makeMaterial(o);

  ossia::mesh_primitive prim;
  prim.vertex_buffers = {pos, nrm, uv};
  auto attr = [&](ossia::attribute_semantic s, ossia::vertex_format f, int buf,
                  int stride) {
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
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

// Plays the part of the Light process: owns a RawLight and a RawTransform
// arena slot, uploads a directional light pointing down local -Z (towards the
// quad, which faces +Z), and stamps both refs on the scene it emits.
struct SceneRenderer final : score::gfx::NodeRenderer
{
  const SceneNode& self;
  ossia::scene_spec m_scene;
  score::gfx::GpuResourceRegistry::Slot m_light, m_xform;

  explicit SceneRenderer(const SceneNode& n)
      : NodeRenderer{n}
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
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res, score::gfx::Edge*) override
  {
    upload(r, res);
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
  void release(score::gfx::RenderList& r) override
  {
    if(m_light.valid())
      r.registry().free(m_light);
    if(m_xform.valid())
      r.registry().free(m_xform);
    m_scene = {};
    m_initialized = false;
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
    const auto* csf = ossia::get_if<isf::csf_image_input>(&in.data);
    const bool image = ossia::get_if<isf::image_input>(&in.data)
                       || ossia::get_if<isf::cubemap_input>(&in.data)
                       || ossia::get_if<isf::texture_input>(&in.data)
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
    if(!ossia::get_if<isf::float_input>(&in.data))
      continue;
    if(in.name == name && k < floatPorts.size())
      setControl(n, floatPorts[k], ossia::value{v});
    ++k;
  }
}

#if defined(FIXO_OPENPBR_BSDF)
// The eight OpenPBR lookup tables, uploaded the way Academy's "OpenPBR LUTs"
// process does, in OpenPBR_LutId order. Without them the preset's energy
// compensation reads zero and its diffuse lobe vanishes.
struct OpenpbrLutNode final : score::gfx::ProcessNode
{
  OpenpbrLutNode()
  {
    for(int i = 0; i < 8; ++i)
      output.push_back(
          new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
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
      QRhi& rhi, QRhiResourceUpdateBatch& res, const std::uint16_t* src, int w,
      int h)
  {
    auto* t = rhi.newTexture(QRhiTexture::R16, QSize(w, h), 1, {});
    t->create();
    QRhiTextureSubresourceUploadDescription sub{
        src, quint32(std::size_t(w) * h * sizeof(std::uint16_t))};
    res.uploadTexture(t, QRhiTextureUploadDescription{{0, 0, sub}});
    return t;
  }
  static QRhiTexture* table3D(
      QRhi& rhi, QRhiResourceUpdateBatch& res, const std::uint16_t* src, int n)
  {
    auto* t = rhi.newTexture(
        QRhiTexture::R16, n, n, n, 1, QRhiTexture::ThreeDimensional);
    t->create();
    const std::size_t slice = std::size_t(n) * n * sizeof(std::uint16_t);
    std::vector<QRhiTextureUploadEntry> entries;
    for(int z = 0; z < n; ++z)
      entries.push_back(QRhiTextureUploadEntry{
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
    using namespace fixo_openpbr;
    auto& rhi = *r.state.rhi;
    constexpr int N = OpenPBR_EnergyTableSize;
    tex[OpenPBR_LutId_IdealDielectricEnergyComplement]
        = table3D(rhi, res, OpenPBR_IdealDielectricEnergyComplement_Array, N);
    tex[OpenPBR_LutId_IdealDielectricAverageEnergyComplement] = table2D(
        rhi, res, OpenPBR_IdealDielectricAverageEnergyComplement_Array, N, N);
    tex[OpenPBR_LutId_IdealDielectricReflectionRatio] = table2D(
        rhi, res, OpenPBR_IdealDielectricReflectionRatio_Array, N, N);
    tex[OpenPBR_LutId_OpaqueDielectricEnergyComplement]
        = table3D(rhi, res, OpenPBR_OpaqueDielectricEnergyComplement_Array, N);
    tex[OpenPBR_LutId_OpaqueDielectricAverageEnergyComplement] = table2D(
        rhi, res, OpenPBR_OpaqueDielectricAverageEnergyComplement_Array, N, N);
    tex[OpenPBR_LutId_IdealMetalEnergyComplement]
        = table2D(rhi, res, OpenPBR_IdealMetalEnergyComplement_Array, N, N);
    tex[OpenPBR_LutId_IdealMetalAverageEnergyComplement] = table2D(
        rhi, res, OpenPBR_IdealMetalAverageEnergyComplement_Array, N, 1);
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
  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
  {
  }
  QRhiTexture* textureForOutput(const score::gfx::Port& output) override
  {
    for(int i = 0; i < 8; ++i)
      if(self.output[i] == &output)
        return tex[i];
    return nullptr;
  }
  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
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
    "openpbr_lut_ideal_dielectric_3d",   "openpbr_lut_ideal_dielectric_avg",
    "openpbr_lut_dielectric_ratio",      "openpbr_lut_opaque_dielectric_3d",
    "openpbr_lut_opaque_dielectric_avg", "openpbr_lut_metal",
    "openpbr_lut_metal_avg",             "openpbr_lut_ltc"};
#endif

struct LibraryRoot
{
  Library::Settings::Model& lib;
  QString previous;
  explicit LibraryRoot(const score::GUIApplicationContext& ctx)
      : lib{ctx.settings<Library::Settings::Model>()}
      , previous{lib.getRootPath()}
  {
    lib.setRootPath(libraryRoot());
  }
  ~LibraryRoot() { lib.setRootPath(previous); }
};

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
    if(auto* f = ossia::get_if<isf::float_input>(&in.data))
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
};

Probe renderScene(
    score::gfx::GraphicsApi api, const char* vs, const char* fs, SceneOpts opts,
    bool openpbrLuts = false)
{
  Probe out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext& ctx) {
    LibraryRoot root{ctx};
    GfxPipeline p;
    auto node = std::make_unique<SceneNode>();
    node->opts = opts;
    const int hn = p.addNode(std::move(node));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
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
#if defined(FIXO_OPENPBR_BSDF)
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
    p.render(5);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      out.err = "readback failed";
      return;
    }
    out.center = img.at(kSize / 2, kSize / 2);
  });
  return out;
}

std::string rgb(std::array<uint8_t, 4> c)
{
  return "(" + std::to_string(c[0]) + "," + std::to_string(c[1]) + ","
         + std::to_string(c[2]) + "," + std::to_string(c[3]) + ")";
}

bool haveRasterPresets()
{
  return QFileInfo::exists(preset("classic_pbr_full.frag"));
}
}

TEST_CASE("blinn_phong is lit by a scene light", "[gfx][presets][fixO]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!haveRasterPresets())
    SKIP("preset library not found at " + presetDir().toStdString());

  SceneOpts dark;
  SceneOpts lit;
  lit.light = true;
  const auto a = renderScene(api, "blinn_phong.vert", "blinn_phong.frag", dark);
  if(a.skipped)
    SKIP("backend unavailable");
  const auto b = renderScene(api, "blinn_phong.vert", "blinn_phong.frag", lit);
  INFO("unlit " << rgb(a.center) << " lit " << rgb(b.center));
  REQUIRE(a.err.empty());
  REQUIRE(b.err.empty());
  // Ambient 0.05 without a light; diffuse 0.8 x N.L = 1 on top with one.
  CHECK(a.center[0] < 40);
  CHECK(b.center[0] > 150);
}

TEST_CASE(
    "classic_pbr_textured draws the material's own colour without a wired array",
    "[gfx][presets][fixO]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!haveRasterPresets())
    SKIP("preset library not found at " + presetDir().toStdString());

  SceneOpts textured;
  textured.base = {1.f, 1.f, 1.f, 1.f};
  textured.redTexture = true;
  SceneOpts plain;
  plain.base = {0.f, 1.f, 0.f, 1.f};
  const auto t = renderScene(
      api, "classic_pbr_textured.vert", "classic_pbr_textured.frag", textured);
  if(t.skipped)
    SKIP("backend unavailable");
  const auto g = renderScene(
      api, "classic_pbr_textured.vert", "classic_pbr_textured.frag", plain);
  INFO("textured " << rgb(t.center) << " plain " << rgb(g.center));
  REQUIRE(t.err.empty());
  REQUIRE(g.err.empty());
  // No light: ambient 0.04 + the fake environment term, both proportional to
  // the base colour, so only its hue is asserted, and full coverage.
  CHECK(t.center[0] > 8);
  CHECK(t.center[0] > 3 * std::max<int>(t.center[1], t.center[2]) );
  CHECK(t.center[3] == 255);
  CHECK(g.center[1] > 8);
  CHECK(g.center[1] > 3 * std::max<int>(g.center[0], g.center[2]));
  CHECK(g.center[3] == 255);
}

TEST_CASE("classic_pbr_shadowed shades a lit quad", "[gfx][presets][fixO]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!haveRasterPresets())
    SKIP("preset library not found at " + presetDir().toStdString());

  SceneOpts lit;
  lit.light = true;
  lit.intensity = 3.f;
  const auto s = renderScene(
      api, "classic_pbr_shadowed.vert", "classic_pbr_shadowed.frag", lit);
  if(s.skipped)
    SKIP("backend unavailable");
  const auto f
      = renderScene(api, "classic_pbr_full.vert", "classic_pbr_full.frag", lit);
  SceneOpts casting = lit;
  casting.castShadow = true;
  const auto c = renderScene(
      api, "classic_pbr_shadowed.vert", "classic_pbr_shadowed.frag", casting);
  INFO("shadowed " << rgb(s.center) << " casting, no cascades " << rgb(c.center)
                   << " full " << rgb(f.center));
  REQUIRE(s.err.empty());
  REQUIRE(c.err.empty());
  REQUIRE(f.err.empty());
  CHECK(s.center[0] > 100);
  CHECK(std::abs(int(s.center[0]) - int(f.center[0])) < 64);
  // A shadow-casting light with no cascades wired is unshadowed.
  CHECK(std::abs(int(c.center[0]) - int(s.center[0])) <= 2);
}

TEST_CASE(
    "classic_pbr_openpbr's default light scale matches classic_pbr_full",
    "[gfx][presets][fixO]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!haveRasterPresets())
    SKIP("preset library not found at " + presetDir().toStdString());

#if !defined(FIXO_OPENPBR_BSDF)
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
  const auto f0
      = renderScene(api, "classic_pbr_full.vert", "classic_pbr_full.frag", dark);
  const auto f1
      = renderScene(api, "classic_pbr_full.vert", "classic_pbr_full.frag", lit);
  INFO("openpbr " << rgb(o0.center) << " -> " << rgb(o1.center) << " | full "
                  << rgb(f0.center) << " -> " << rgb(f1.center));
  REQUIRE(o0.err.empty());
  REQUIRE(o1.err.empty());
  REQUIRE(f0.err.empty());
  REQUIRE(f1.err.empty());

  const int dOpen = int(o1.center[0]) - int(o0.center[0]);
  const int dFull = int(f1.center[0]) - int(f0.center[0]);
  // The light's own contribution on a 0.5 grey diffuse quad is ~0.15 through
  // classic_pbr_full; OpenPBR's energy-conserving diffuse lands within a few
  // percent of it.
  CHECK(dFull > 20);
  CHECK(dOpen > 0.75 * dFull);
  CHECK(dOpen < 1.33 * dFull);
#endif
}

namespace
{
struct CubeProbe
{
  bool skipped = false;
  std::string err;
  ReadbackImage img;
};

// The corpus cube writer paints +Y (0.25, 0.85, 0.25) and -Y
// (0.85, 0.85, 0.25): red tells the two poles apart.
CubeProbe renderCubeViewer(
    score::gfx::GraphicsApi api, const QString& viewer,
    std::vector<ControlSetting> controls)
{
  CubeProbe out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      out.skipped = true;
      out.err = why;
      return;
    }
    GfxPipeline p;
    const int cube = p.addCsf(
        QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-cube-image-write.cs"));
    const int view = p.addIsf(viewer);
    if(cube < 0 || view < 0)
    {
      out.err = "chain build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(cube, 0), p.imageIn(view, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.err = out.skipped ? std::string{} : p.error();
      return;
    }
    applyZeroDefaults(*p.isf(view));
    for(auto& c : controls)
      setControl(*p.isf(view), c.port, c.value);
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
    "[gfx][presets][fixO][cubemap]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!QFileInfo::exists(preset("cubemap_orbit.fs")))
    SKIP("preset library not found at " + presetDir().toStdString());
  // Port 3 is fov: at 140 degrees the top and bottom rows look past the
  // horizon into the poles.
  const auto orbit = renderCubeViewer(
      api, preset("cubemap_orbit.fs"), {{3, ossia::value{140.f}}});
  if(orbit.skipped)
    SKIP("backend unavailable: " + orbit.err);
  const auto view = renderCubeViewer(api, preset("cubemap_view.fs"), {});
  REQUIRE(orbit.err.empty());
  REQUIRE(view.err.empty());

  const auto oTop = orbit.img.at(kSize / 2, 1);
  const auto oBot = orbit.img.at(kSize / 2, kSize - 2);
  const auto vTop = view.img.at(kSize / 2, 1);
  const auto vBot = view.img.at(kSize / 2, kSize - 2);
  INFO("orbit top " << rgb(oTop) << " bottom " << rgb(oBot) << " | view top "
                    << rgb(vTop) << " bottom " << rgb(vBot));
  const auto plusY = [](auto c) { return c[1] > 180 && c[0] < 110; };
  const auto minusY = [](auto c) { return c[1] > 180 && c[0] > 180; };
  CHECK(plusY(vTop));
  CHECK(minusY(vBot));
  CHECK(plusY(oTop));
  CHECK(minusY(oBot));
}

TEST_CASE(
    "the stock compute presets fit a 256-invocation workgroup",
    "[gfx][presets][fixO][compute]")
{
  const QDir dir(computePresetDir());
  const auto files = dir.entryList({QStringLiteral("*.cs")}, QDir::Files);
  if(files.isEmpty())
    SKIP("compute presets not found at " + computePresetDir().toStdString());
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
    const int life
        = p.addCsf(QDir(computePresetDir()).filePath("Game of Life.cs"));
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
      tk.parent_duration = ossia::time_value{
          int64_t(1000 * ossia::flicks_per_second<double>)};
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
    "[gfx][presets][fixO][compute]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!QFileInfo::exists(QDir(computePresetDir()).filePath("Game of Life.cs")))
    SKIP("compute presets not found at " + computePresetDir().toStdString());
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
    const int src = p.addCsf(
        QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromLatin1(producer));
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
    "cubemap_array_orbit.frag shows +Y at the top",
    "[gfx][presets][fixO][cubemap]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  if(!QFileInfo::exists(preset("cubemap_array_orbit.frag")))
    SKIP("preset library not found at " + presetDir().toStdString());

  // At a 140 degree fov the top and bottom rows look past the horizon into
  // the poles; the faces are flat tints, so only the direction matters.
  const auto r = renderViewer(
      api, "fixo-cube-array-faces.cs", preset("cubemap_array_orbit.vert"),
      preset("cubemap_array_orbit.frag"), "cube_array",
      {{"fov", 140.f}, {"gamma", 1.f}});
  if(r.skipped)
    SKIP("backend unavailable: " + r.err);
  INFO(r.err);
  REQUIRE(r.err.empty());
  const auto top = r.img.at(kSize / 2, 1);
  const auto bottom = r.img.at(kSize / 2, kSize - 2);
  INFO("top " << rgb(top) << " bottom " << rgb(bottom));
  CHECK((top[1] > 180 && top[0] < 110));
  CHECK((bottom[1] > 180 && bottom[0] > 180));
}

TEST_CASE(
    "probe_voxel_orbit_view.csf shows +Y at the top and +X on the right",
    "[gfx][presets][fixO][voxel]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  const QString viewer = QDir(presetDir()).filePath(
      QStringLiteral("../lighting/probe_voxel_orbit_view.csf"));
  if(!QFileInfo::exists(viewer))
    SKIP("preset library not found at " + presetDir().toStdString());

  // Yaw pi/2, pitch 0: the camera sits on +Z looking down -Z, so +X is on the
  // right. The grid is red above y = 0.15, green at x > 0.15 below it and
  // blue at x < -0.15 below it.
  const auto r = renderViewer(
      api, "fixo-voxel-markers.cs", {}, viewer, "voxel_grid",
      {{"yaw", 1.5707963f}, {"pitch", 0.f}});
  if(r.skipped)
    SKIP("backend unavailable: " + r.err);
  INFO(r.err);
  REQUIRE(r.err.empty());
  const auto top = r.img.at(kSize / 2, 24);
  const auto right = r.img.at(37, kSize / 2);
  const auto left = r.img.at(26, kSize / 2);
  const auto bottom = r.img.at(kSize / 2, 40);
  INFO("top " << rgb(top) << " right " << rgb(right) << " left " << rgb(left)
              << " bottom " << rgb(bottom));
  const auto dominant = [](auto c, int ch) {
    return c[ch] > 60 && c[ch] > 2 * c[(ch + 1) % 3] && c[ch] > 2 * c[(ch + 2) % 3];
  };
  CHECK(dominant(top, 0));
  CHECK(dominant(right, 1));
  CHECK(dominant(left, 2));
  CHECK(!dominant(bottom, 0));
}
