// Switching a texture inlet's mipmaps on or off while the graph runs
// reallocates the inlet's render target (agent L4).
//
// - ISF inlet: the render target gains / loses MipMapped |
//   UsedWithGenerateMips, and a textureLod(3) probe of a one-pixel
//   checkerboard reads 0.5 grey only while a generated mip chain exists.
// - avnd GPU texture inputs (a halp::gpu_texture_input filter) and gpp shader
//   node inputs are rebuilt with a mipmapped target; the gpp node samples the
//   generated mips.
//
// Registration: see the test_gfx_mip_toggle_l4 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/GpuNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <gpp/layout.hpp>
#include <gpp/meta.hpp>
#include <gpp/ports.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct LodInput
{
  halp_meta(name, "L4 lod input")
  halp_meta(uuid, "b6e1f0a3-5c27-4d9e-8a14-2f7c3e9d6b51")

  struct layout
  {
    enum
    {
      graphics
    };

    struct fragment_output
    {
      gpp_attribute(0, fragColor, float[4])
      fragColor;
    } fragment_output;

    struct bindings
    {
      gpp::sampler<"tex", 1> tex;
    };
  };

  struct
  {
    gpp::texture_input_port<"In", &layout::bindings::tex> in;
  } inputs;

  struct
  {
    gpp::color_attachment_port<"Out", &layout::fragment_output> out;
  } outputs;

  std::string_view fragment()
  {
    return R"_(
void main() {
  fragColor = vec4(textureLod(tex, vec2(32.5 / 64.0), 3.0).rgb, 1.0);
}
)_";
  }
};

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

score::gfx::Port* firstInput(score::gfx::Node& n, score::gfx::Types type)
{
  for(auto* port : n.input)
    if(port->type == type)
      return port;
  return nullptr;
}

ossia::render_target_spec mipSpec(ossia::texture_filter mip)
{
  ossia::render_target_spec s;
  s.mipmap_mode = mip;
  return s;
}

ossia::render_target_spec sizedMipSpec(int w, int h, ossia::texture_filter mip)
{
  ossia::render_target_spec s = mipSpec(mip);
  s.size = ossia::texture_size{w, h};
  return s;
}

struct TexState
{
  bool present{};
  bool mipmapped{};
  bool generatesMips{};
  QSize size;
};

TexState stateOf(const QRhiTexture* tex)
{
  TexState s;
  if(!tex)
    return s;
  s.present = true;
  s.mipmapped = tex->flags().testFlag(QRhiTexture::MipMapped);
  s.generatesMips = tex->flags().testFlag(QRhiTexture::UsedWithGenerateMips);
  s.size = tex->pixelSize();
  return s;
}

struct Pixels
{
  int grey{};
  int extreme{};
  int total{};
};

Pixels classify(const ReadbackImage& img)
{
  Pixels p;
  if(!img.valid())
    return p;
  for(int y = 0; y < img.height; y++)
    for(int x = 0; x < img.width; x++)
    {
      const int r = img.at(x, y)[0];
      p.total++;
      if(r >= 120 && r <= 136)
        p.grey++;
      else if(r <= 8 || r >= 247)
        p.extreme++;
    }
  return p;
}

struct IsfToggle
{
  bool skipped{};
  std::string skipReason;
  std::string error;
  TexState before, on, off;
  Pixels pxBefore, pxOn, pxOff;
};

// checker -> lod probe (spec'd inlet) -> sink 64x64
IsfToggle runIsfToggle(score::gfx::GraphicsApi api)
{
  IsfToggle out;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int a = p.addIsf(corpus("l4-mip-checker.fs"));
    const int x = p.addIsf(corpus("l4-mip-lod-probe.fs"));
    if(a < 0 || x < 0)
    {
      out.error = "build failed: " + p.error();
      return;
    }
    const int s = p.addSink({64, 64});
    p.wire(p.imageOut(a, 0), p.imageIn(x, 0));
    p.wire(p.imageOut(x, 0), p.sinkInput(s));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.skipReason = p.skipReason();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }
    const auto& rls = p.graph().renderLists();
    if(rls.empty() || !rls.front())
    {
      out.error = "no render list";
      return;
    }
    auto& rl = *rls.front();
    auto* port = p.imageIn(x, 0);
    const int portIdx = first_image_input(*p.isf(x));

    p.render(3);
    out.before = stateOf(rl.renderTargetForInputPort(*port).texture);
    out.pxBefore = classify(p.readback(s));

    setRenderTargetSpec(*p.isf(x), portIdx, mipSpec(ossia::texture_filter::LINEAR));
    p.render(3);
    out.on = stateOf(rl.renderTargetForInputPort(*port).texture);
    out.pxOn = classify(p.readback(s));

    setRenderTargetSpec(*p.isf(x), portIdx, mipSpec(ossia::texture_filter::NONE));
    p.render(3);
    out.off = stateOf(rl.renderTargetForInputPort(*port).texture);
    out.pxOff = classify(p.readback(s));
  });
  return out;
}

struct GpuInletSeen
{
  QRhiTexture* handle{};
  QRhiSampler* sampler{};
} g_inlet;

struct GpuInletProbe
{
  halp_meta(name, "L4 GPU inlet probe")
  halp_meta(c_name, "l4_gpu_inlet_probe")
  halp_meta(category, "Test")
  halp_meta(uuid, "4d8f2a61-9b3e-4c75-a0d2-7e1b5c9f3a84")

  struct
  {
    halp::gpu_texture_input<"In"> in;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Out"> out;
  } outputs;

  void operator()()
  {
    g_inlet.handle = static_cast<QRhiTexture*>(inputs.in.texture.handle);
    g_inlet.sampler = static_cast<QRhiSampler*>(inputs.in.texture.sampler_handle);
  }
};

struct AvndToggle
{
  bool skipped{};
  std::string error;
  TexState before, on;
  QRhiSampler::Filter mipBefore{}, mipOn{};
};

// checker -> probe (halp::gpu_texture_input) -> passthrough -> sink
AvndToggle runAvndToggle(score::gfx::GraphicsApi api)
{
  AvndToggle out;
  g_inlet = {};
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

    const int prod = p.addIsf(corpus("l4-mip-checker.fs"));
    const int probe = p.addNode(procs.make<GpuInletProbe>(ctx));
    const int pass = p.addIsf(corpus("isf-passthrough-plain.fs"));
    if(prod < 0 || probe < 0 || pass < 0)
    {
      out.error = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(prod, 0), firstInput(*p.node(probe), score::gfx::Types::Image));
    p.wire(p.nodeImageOut(probe, 0), p.imageIn(pass, 0));
    p.wire(p.imageOut(pass, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }

    p.render(4);
    out.before = stateOf(g_inlet.handle);
    if(g_inlet.sampler)
      out.mipBefore = g_inlet.sampler->mipmapMode();

    p.node(probe)->process(0, mipSpec(ossia::texture_filter::LINEAR));
    p.render(4);
    out.on = stateOf(g_inlet.handle);
    if(g_inlet.sampler)
      out.mipOn = g_inlet.sampler->mipmapMode();
  });
  return out;
}

struct GppToggle
{
  bool skipped{};
  std::string error;
  TexState before, on;
  std::array<uint8_t, 4> pxBefore{}, pxOn{};
};

// checker -> gpp node sampling mip level 3 (64x64 inlet) -> sink 16x16
GppToggle runGppToggle(score::gfx::GraphicsApi api)
{
  GppToggle out;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      out.error = "no document";
      return;
    }
    GfxPipeline p;
    const int prod = p.addIsf(corpus("l4-mip-checker.fs"));
    auto owned = std::make_unique<oscr::CustomGpuNode<LodInput>>(
        std::weak_ptr<Execution::ExecutionCommandQueue>{}, Gfx::exec_controls{}, 1,
        doc->context());
    auto* gpp = owned.get();
    setRenderTargetSpec(*gpp, 0, sizedMipSpec(64, 64, ossia::texture_filter::NONE));
    const int idx = p.addNode(std::move(owned));
    if(prod < 0 || idx < 0)
    {
      out.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.imageOut(prod, 0), gpp->input[0]);
    const int sink = p.addSink({16, 16});
    p.wire(gpp->output[0], p.sinkInput(sink));
    if(!p.create(api))
    {
      out.skipped = p.skipped();
      out.error = out.skipped ? std::string{} : p.error();
      return;
    }

    auto sample = [&](TexState& st, std::array<uint8_t, 4>& px) {
      p.render(3);
      auto img = p.readback(sink);
      REQUIRE(img.valid());
      px = img.center();
      REQUIRE(!gpp->renderedNodes.empty());
      auto* rn = dynamic_cast<oscr::CustomGpuRenderer<LodInput>*>(
          gpp->renderedNodes.begin()->second);
      REQUIRE(rn);
      auto rt = rn->m_rts.find(gpp->input[0]);
      REQUIRE(rt != rn->m_rts.end());
      st = stateOf(rt->second.texture);
    };

    sample(out.before, out.pxBefore);
    setRenderTargetSpec(*gpp, 0, sizedMipSpec(64, 64, ossia::texture_filter::LINEAR));
    sample(out.on, out.pxOn);
  });
  return out;
}
}

TEST_CASE(
    "Switching an ISF inlet's mipmaps while running reallocates its target",
    "[gfx][texture][inlet-settings][mipmap][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const IsfToggle r = runIsfToggle(api);
  if(r.skipped)
    SKIP(r.skipReason);
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());

  REQUIRE(r.before.present);
  CHECK(!r.before.mipmapped);
  CHECK(r.before.size == QSize(64, 64));

  REQUIRE(r.on.present);
  CHECK(r.on.mipmapped);
  CHECK(r.on.generatesMips);
  CHECK(r.on.size == QSize(64, 64));

  REQUIRE(r.off.present);
  CHECK(!r.off.mipmapped);
  CHECK(!r.off.generatesMips);

  if(api == score::gfx::Null)
    return;

  INFO(
      "grey/extreme/total before " << r.pxBefore.grey << "/" << r.pxBefore.extreme
                                   << "/" << r.pxBefore.total << " on " << r.pxOn.grey
                                   << "/" << r.pxOn.extreme << " off " << r.pxOff.grey
                                   << "/" << r.pxOff.extreme);
  REQUIRE(r.pxBefore.total == 64 * 64);
  CHECK(r.pxBefore.extreme == r.pxBefore.total);
  CHECK(r.pxOn.grey == r.pxOn.total);
  CHECK(r.pxOff.extreme == r.pxOff.total);
}

TEST_CASE(
    "Switching an avnd GPU texture input's mipmaps while running",
    "[gfx][avnd][texture][inlet-settings][mipmap][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const AvndToggle r = runAvndToggle(api);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.before.present);
  CHECK(!r.before.mipmapped);
  CHECK(r.mipBefore == QRhiSampler::None);
  REQUIRE(r.on.present);
  CHECK(r.on.mipmapped);
  CHECK(r.on.generatesMips);
  CHECK(r.mipOn == QRhiSampler::Linear);
}

TEST_CASE(
    "Switching a gpp shader node input's mipmaps while running",
    "[gfx][avnd][gpp][texture][inlet-settings][mipmap][l4]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const GppToggle r = runGppToggle(api);
  if(r.skipped)
    SKIP("backend unavailable");
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.before.present);
  CHECK(!r.before.mipmapped);
  CHECK(r.before.size == QSize(64, 64));
  REQUIRE(r.on.present);
  CHECK(r.on.mipmapped);
  CHECK(r.on.size == QSize(64, 64));

  if(api == score::gfx::Null)
    return;
  INFO(
      "center before " << int(r.pxBefore[0]) << " on " << int(r.pxOn[0]));
  CHECK((r.pxBefore[0] <= 8 || r.pxBefore[0] >= 247));
  CHECK(int(r.pxOn[0]) >= 120);
  CHECK(int(r.pxOn[0]) <= 136);
}
