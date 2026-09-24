// How the avnd bridge resolves a CPU halp node's texture, scene and geometry
// inlets.
//
// Texture inlets (N77): an unwired gpu_texture_input is null, not a render
// target of its own. A wired one is the upstream's own texture when the
// upstream publishes one (textureForOutput), with that texture's size and
// kind, so a cube reaches Scene Resource Route as a cube; otherwise it is the
// inlet's render target the upstream draws into. A CPU texture inlet reads
// the upstream texture back at the upstream's size instead of the render
// target's.
//
// Scene inlets (N78): a node with several scene inlets gets on each only the
// scene cabled into that inlet, so Scene Switch picks by index.
//
// Geometry inlets (N87): a CPU mesh reaching a dynamic_gpu_geometry inlet
// carries the uploaded GPU buffers on every frame, not only on the frame its
// buffers were dirty, so PBR Mesh draws a Cube primitive.
//
// Geometry transforms (N79b): the transform an upstream sends with its
// geometry reaches the halp geometry input's transform[16] with
// dirty_transform set, so Mesh Noise passes a moved primitive on moved.
//
// Registration: see the test_gfx_avnd_inlet_resolve_fixe target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <Threedim/Noise.hpp>
#include <Threedim/PBRMesh.hpp>
#include <Threedim/Primitive.hpp>
#include <Threedim/SceneResourceRoute.hpp>
#include <Threedim/SceneSwitch.hpp>

#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
void* g_published[2]{};

template <int Tag, bool Cube>
struct PublishedTexture
{
  halp_meta(name, "Published texture")
  halp_meta(c_name, "fixe_published_texture")
  halp_meta(category, "Test")
  halp_meta(
      uuid, Cube ? "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c20"
                 : "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c21")

  struct
  {
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Texture"> texture;
  } outputs;

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) { }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge*)
  {
    if(m_tex)
      return;
    QRhiTexture::Flags flags = QRhiTexture::UsedAsTransferSource;
    if(Cube)
      flags |= QRhiTexture::CubeMap;
    m_tex = renderer.state.rhi->newTexture(QRhiTexture::RGBA8, QSize{4, 4}, 1, flags);
    m_tex->create();
    if(!Cube)
    {
      std::vector<uint8_t> px(4 * 4 * 4);
      for(std::size_t i = 0; i < px.size(); i += 4)
      {
        px[i] = 255;
        px[i + 3] = 255;
      }
      res.uploadTexture(
          m_tex, QRhiTextureUploadEntry{
                     0, 0, QRhiTextureSubresourceUploadDescription{
                               px.data(), quint32(px.size())}});
    }
    outputs.texture.texture.handle = m_tex;
    outputs.texture.texture.width = 4;
    outputs.texture.texture.height = 4;
    g_published[Tag] = m_tex;
  }

  void release(score::gfx::RenderList&)
  {
    delete m_tex;
    m_tex = nullptr;
    outputs.texture.texture.handle = nullptr;
  }

  void runInitialPasses(
      score::gfx::RenderList&, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge&)
  {
  }

  void operator()() { }

  QRhiTexture* m_tex{};
};
using Red = PublishedTexture<0, false>;
using CubeTex = PublishedTexture<1, true>;

struct GpuSeen
{
  bool ran{};
  halp::gpu_texture a, b;
} g_gpu;

struct GpuInletProbe
{
  halp_meta(name, "GPU inlet probe")
  halp_meta(c_name, "fixe_gpu_inlet_probe")
  halp_meta(category, "Test")
  halp_meta(uuid, "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c22")

  struct
  {
    halp::gpu_texture_input<"A"> a;
    halp::gpu_texture_input<"B"> b;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Out"> out;
  } outputs;

  void operator()()
  {
    g_gpu.ran = true;
    g_gpu.a = inputs.a.texture;
    g_gpu.b = inputs.b.texture;
  }
};

struct CpuSeen
{
  bool ran{};
  int width{}, height{};
  halp::custom_variable_texture::texture_format format{};
  int r{-1}, g{-1};
} g_cpu;

struct CpuInletProbe
{
  halp_meta(name, "CPU inlet probe")
  halp_meta(c_name, "fixe_cpu_inlet_probe")
  halp_meta(category, "Test")
  halp_meta(uuid, "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c23")

  struct
  {
    halp::texture_input<"T", halp::custom_variable_texture> tex;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Out"> out;
  } outputs;

  void operator()()
  {
    const auto& t = inputs.tex.texture;
    if(!t.bytes)
      return;
    g_cpu.ran = true;
    g_cpu.width = t.width;
    g_cpu.height = t.height;
    g_cpu.format = t.format;
    g_cpu.r = t.bytes[0];
    g_cpu.g = t.bytes[1];
  }
};

const ossia::scene_state* g_scene_src[2]{};

template <int Tag>
struct SceneSource
{
  halp_meta(name, "Scene source")
  halp_meta(c_name, "fixe_scene_source")
  halp_meta(category, "Test")
  halp_meta(
      uuid, Tag == 0 ? "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c24"
                     : "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c25")

  struct
  {
  } inputs;

  struct
  {
    struct
    {
      halp_meta(name, "Scene");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  void operator()()
  {
    if(!m_state)
    {
      m_state = std::make_shared<ossia::scene_state>();
      m_state->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
      m_state->version = 1;
    }
    g_scene_src[Tag] = m_state.get();
    outputs.scene_out.scene.state = m_state;
    outputs.scene_out.dirty = 0xFF;
  }

  std::shared_ptr<ossia::scene_state> m_state;
};

struct SceneSeen
{
  bool ran{};
  ossia::scene_spec s0, s1;
} g_scenes;

struct SceneInletProbe
{
  halp_meta(name, "Scene inlet probe")
  halp_meta(c_name, "fixe_scene_inlet_probe")
  halp_meta(category, "Test")
  halp_meta(uuid, "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c26")

  struct
  {
    struct
    {
      halp_meta(name, "S0");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } s0;
    struct
    {
      halp_meta(name, "S1");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } s1;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Out"> out;
  } outputs;

  void operator()()
  {
    g_scenes.ran = true;
    g_scenes.s0 = inputs.s0.scene;
    g_scenes.s1 = inputs.s1.scene;
  }
};

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

struct Run
{
  bool skipped{};
  std::string error;
  ReadbackImage img;
};

// Builds `build(p, procs, ctx)`, which returns the node whose image output
// ends the chain (an addNode index), then renders it into a sink.
template <typename Build>
Run run_chain(score::gfx::GraphicsApi api, Build&& build, bool isfTail = false)
{
  Run out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    const auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;
    const int last = build(p, procs, ctx);
    if(last < 0 || !p.error().empty())
    {
      out.error = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    if(isfTail)
    {
      p.wire(p.imageOut(last, 0), p.sinkInput(sink));
    }
    else
    {
      const int pass = p.addIsf(corpus("isf-passthrough-plain.fs"));
      p.wire(p.nodeImageOut(last, 0), p.imageIn(pass, 0));
      p.wire(p.imageOut(pass, 0), p.sinkInput(sink));
    }
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    out.img = p.readback(sink);
  });
  return out;
}

score::gfx::Port* imageInput(GfxPipeline& p, int node, int k)
{
  int seen = 0;
  for(auto* port : p.node(node)->input)
    if(port->type == score::gfx::Types::Image && seen++ == k)
      return port;
  return nullptr;
}
}

TEST_CASE(
    "an unwired gpu texture inlet is null, a wired one is the upstream texture",
    "[gfx][avnd][texture][fixE]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  g_gpu = {};
  g_published[0] = nullptr;

  const Run r = run_chain(api, [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int src = p.addNode(procs.make<Red>(ctx));
    const int probe = p.addNode(procs.make<GpuInletProbe>(ctx));
    p.wire(p.nodeImageOut(src, 0), imageInput(p, probe, 0));
    return probe;
  });
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(g_gpu.ran);

  CHECK(g_gpu.a.handle == g_published[0]);
  CHECK(g_gpu.a.width == 4);
  CHECK(g_gpu.a.height == 4);
  CHECK(g_gpu.a.kind == halp::texture_kind::texture_2d);

  CHECK(g_gpu.b.handle == nullptr);
  CHECK(g_gpu.b.width == 0);
  CHECK(g_gpu.b.height == 0);
}

TEST_CASE(
    "a gpu texture inlet fed by a shader is the render target it draws into",
    "[gfx][avnd][texture][fixE]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  g_gpu = {};

  const Run r = run_chain(api, [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int solid = p.addIsf(corpus("isf-solid-color.fs"));
    const int probe = p.addNode(procs.make<GpuInletProbe>(ctx));
    p.wire(p.imageOut(solid, 0), imageInput(p, probe, 1));
    return probe;
  });
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(g_gpu.ran);

  CHECK(g_gpu.a.handle == nullptr);
  CHECK(g_gpu.b.handle != nullptr);
  CHECK(g_gpu.b.width > 0);
  CHECK(g_gpu.b.kind == halp::texture_kind::texture_2d);
}

TEST_CASE(
    "Scene Resource Route routes the cube it receives, never a 2D texture, as skybox",
    "[gfx][avnd][texture][fixE]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  // Cube upstream: routed as the skybox, and the handle is the cube itself.
  g_scenes = {};
  g_published[1] = nullptr;
  const Run cube = run_chain(api, [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int src = p.addNode(procs.make<CubeTex>(ctx));
    const int route = p.addNode(procs.make<Threedim::SceneResourceRoute>(ctx));
    const int probe = p.addNode(procs.make<SceneInletProbe>(ctx));
    p.wire(p.nodeImageOut(src, 0), imageInput(p, route, 0));
    p.wire(p.nodeSceneOut(route, 0), p.nodeSceneIn(probe, 0));
    return probe;
  });
  if(cube.skipped)
    SKIP("backend unavailable");
  INFO("error=" << cube.error);
  REQUIRE(cube.error.empty());
  REQUIRE(g_scenes.ran);
  REQUIRE(g_scenes.s0.state);
  REQUIRE(g_published[1]);
  CHECK(g_scenes.s0.state->environment.skybox_texture.native_handle == g_published[1]);

  // 2D upstream: nothing is routed to the samplerCube skybox.
  g_scenes = {};
  const Run flat = run_chain(api, [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int solid = p.addIsf(corpus("isf-solid-color.fs"));
    const int route = p.addNode(procs.make<Threedim::SceneResourceRoute>(ctx));
    const int probe = p.addNode(procs.make<SceneInletProbe>(ctx));
    p.wire(p.imageOut(solid, 0), imageInput(p, route, 0));
    p.wire(p.nodeSceneOut(route, 0), p.nodeSceneIn(probe, 0));
    return probe;
  });
  INFO("error=" << flat.error);
  REQUIRE(flat.error.empty());
  REQUIRE(g_scenes.ran);
  REQUIRE(g_scenes.s0.state);
  CHECK(g_scenes.s0.state->environment.skybox_texture.native_handle == nullptr);
}

TEST_CASE(
    "a CPU texture inlet reads the upstream texture at the upstream size",
    "[gfx][avnd][texture][fixE]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  g_cpu = {};

  const Run r = run_chain(api, [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int src = p.addNode(procs.make<Red>(ctx));
    const int probe = p.addNode(procs.make<CpuInletProbe>(ctx));
    p.wire(p.nodeImageOut(src, 0), imageInput(p, probe, 0));
    return probe;
  });
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(g_cpu.ran);
  CHECK(g_cpu.width == 4);
  CHECK(g_cpu.height == 4);
  CHECK(g_cpu.format == halp::custom_variable_texture::RGBA8);
  CHECK(g_cpu.r == 255);
  CHECK(g_cpu.g == 0);
}

TEST_CASE(
    "each scene inlet of a halp node receives only its own cable",
    "[gfx][avnd][scene][fixE]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  g_scenes = {};
  g_scene_src[0] = g_scene_src[1] = nullptr;
  const Run direct = run_chain(api, [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int a = p.addNode(procs.make<SceneSource<0>>(ctx));
    const int b = p.addNode(procs.make<SceneSource<1>>(ctx));
    const int probe = p.addNode(procs.make<SceneInletProbe>(ctx));
    p.wire(p.nodeSceneOut(a, 0), p.nodeSceneIn(probe, 0));
    p.wire(p.nodeSceneOut(b, 0), p.nodeSceneIn(probe, 1));
    return probe;
  });
  if(direct.skipped)
    SKIP("backend unavailable");
  INFO("error=" << direct.error);
  REQUIRE(direct.error.empty());
  REQUIRE(g_scenes.ran);
  REQUIRE(g_scene_src[0]);
  REQUIRE(g_scene_src[1]);
  CHECK(g_scenes.s0.state.get() == g_scene_src[0]);
  CHECK(g_scenes.s1.state.get() == g_scene_src[1]);

  // Scene Switch at its default index 0 forwards what is cabled into Scene 0
  // (source 1 here), not the union of its inputs.
  g_scenes = {};
  g_scene_src[0] = g_scene_src[1] = nullptr;
  const Run sw = run_chain(api, [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int a = p.addNode(procs.make<SceneSource<0>>(ctx));
    const int b = p.addNode(procs.make<SceneSource<1>>(ctx));
    const int sw = p.addNode(procs.make<Threedim::SceneSwitch>(ctx));
    const int probe = p.addNode(procs.make<SceneInletProbe>(ctx));
    p.wire(p.nodeSceneOut(b, 0), p.nodeSceneIn(sw, 0));
    p.wire(p.nodeSceneOut(a, 0), p.nodeSceneIn(sw, 1));
    p.wire(p.nodeSceneOut(sw, 0), p.nodeSceneIn(probe, 0));
    return probe;
  });
  INFO("error=" << sw.error);
  REQUIRE(sw.error.empty());
  REQUIRE(g_scenes.ran);
  REQUIRE(g_scene_src[1]);
  CHECK(g_scenes.s0.state.get() == g_scene_src[1]);
}

TEST_CASE(
    "PBR Mesh draws a CPU primitive",
    "[gfx][avnd][geometry][fixE]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Run r = run_chain(
      api,
      [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    const int mesh = p.addNode(procs.make<Threedim::PBRMesh>(ctx));
    const int flat = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster
        = p.addRaster(corpus("rr-sinkdepth-greater.vs"), corpus("rr-sinkdepth-greater.fs"));
    if(raster < 0)
      return -1;
    int geo = 0;
    score::gfx::Port* meshIn{};
    for(auto* port : p.node(mesh)->input)
      if(port->type == score::gfx::Types::Geometry && geo++ == 0)
        meshIn = port;
    p.wire(p.nodeGeometryOut(cube, 0), meshIn);
    p.wire(p.nodeSceneOut(mesh, 0), p.nodeSceneIn(flat, 0));
    p.wire(p.nodeGeometryOut(flat, 0), p.geometryIn(raster, 0));
    return raster;
  },
      true);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.img.valid());
  int lit = 0;
  for(int y = 0; y < r.img.height; ++y)
    for(int x = 0; x < r.img.width; ++x)
    {
      const auto px = r.img.at(x, y);
      lit += (int(px[0]) + int(px[1]) + int(px[2])) > 128 ? 1 : 0;
    }
  INFO("lit " << lit << " of " << r.img.width * r.img.height);
  CHECK(lit > 20);
}

TEST_CASE(
    "PBR Mesh cabled to a CPU primitive that already rendered publishes a mesh",
    "[gfx][avnd][geometry][fixE]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  g_scenes = {};
  bool skipped = false;
  std::string err;
  bool emptyBefore = false;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      err = "no document";
      return;
    }
    const auto& ctx = doc->context();
    HalpProcesses procs;
    GfxPipeline p;
    const int cube = p.addNode(procs.make<Threedim::Cube>(ctx));
    const int mesh = p.addNode(procs.make<Threedim::PBRMesh>(ctx));
    const int probe = p.addNode(procs.make<SceneInletProbe>(ctx));
    score::gfx::Port* meshIn{};
    for(auto* port : p.node(mesh)->input)
      if(port->type == score::gfx::Types::Geometry)
      {
        meshIn = port;
        break;
      }
    p.wire(p.nodeSceneOut(mesh, 0), p.nodeSceneIn(probe, 0));
    p.wire(p.nodeGeometryOut(cube, 0), p.nodeSceneIn(probe, 1));
    const int pass = p.addIsf(corpus("isf-passthrough-plain.fs"));
    p.wire(p.nodeImageOut(probe, 0), p.imageIn(pass, 0));
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(pass, 0), p.sinkInput(sink));
    if(!meshIn || !p.error().empty())
    {
      err = "build failed: " + p.error();
      return;
    }
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    emptyBefore = g_scenes.ran && !g_scenes.s0.state && g_scenes.s1.state;

    p.addEdgeIncremental(p.nodeGeometryOut(cube, 0), meshIn);
    g_scenes = {};
    p.render(4);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  REQUIRE(emptyBefore);
  REQUIRE(g_scenes.ran);
  REQUIRE(g_scenes.s0.state);
  REQUIRE(g_scenes.s0.state->roots);
  CHECK(g_scenes.s0.state->roots->size() == 1);
}

namespace
{
struct MovedCube : Threedim::Cube
{
  halp_meta(name, "Moved cube")
  halp_meta(c_name, "fixe_moved_cube")
  halp_meta(uuid, "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c27")

  void operator()()
  {
    inputs.position.value.x = 1.f;
    inputs.position.value.y = 2.f;
    inputs.position.value.z = 3.f;
    apply_transform(inputs, outputs.geometry);
  }
};

struct TransformSeen
{
  bool ran{};
  bool dirty{};
  float transform[16]{};
} g_xform;

struct GeometryInletProbe
{
  halp_meta(name, "Geometry inlet probe")
  halp_meta(c_name, "fixe_geometry_inlet_probe")
  halp_meta(category, "Test")
  halp_meta(uuid, "0e9a4c55-51d3-4b0b-9c2e-3f7d8a1b6c28")

  struct
  {
    struct : Threedim::GeometryPort
    {
      halp_meta(name, "Geometry");
    } geometry;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Out"> out;
  } outputs;

  void operator()()
  {
    g_xform.ran = true;
    if(inputs.geometry.dirty_transform)
    {
      g_xform.dirty = true;
      std::copy_n(inputs.geometry.transform, 16, g_xform.transform);
    }
  }
};
}

TEST_CASE(
    "Mesh Noise passes the upstream geometry transform downstream",
    "[gfx][avnd][geometry][fixE]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  g_xform = {};

  const Run r = run_chain(api, [](GfxPipeline& p, HalpProcesses& procs, auto& ctx) {
    const int cube = p.addNode(procs.make<MovedCube>(ctx));
    const int noise = p.addNode(procs.make<Threedim::Noise>(ctx));
    const int probe = p.addNode(procs.make<GeometryInletProbe>(ctx));
    p.wire(p.nodeGeometryOut(cube, 0), p.nodeGeometryIn(noise, 0));
    p.wire(p.nodeGeometryOut(noise, 0), p.nodeGeometryIn(probe, 0));
    return probe;
  });
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(g_xform.ran);
  REQUIRE(g_xform.dirty);
  CHECK(g_xform.transform[12] == 1.f);
  CHECK(g_xform.transform[13] == 2.f);
  CHECK(g_xform.transform[14] == 3.f);
  CHECK(g_xform.transform[15] == 1.f);
}
