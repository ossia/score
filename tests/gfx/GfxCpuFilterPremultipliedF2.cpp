// Translucent colour through avnd texture nodes is multiplied by alpha once.
//
// Render targets between nodes hold premultiplied colour. A CPU filter read its
// input target back as is, handed the premultiplied texels to the process as if
// they were straight colour, and drew the re-uploaded result with the straight
// "over": a pass-through filter turned a stored (64, 0, 0, a 128) into
// (32, 0, 0, a 128). The readback of the node's own input target is now
// unpremultiplied, so the process sees straight colour as ISF shaders do.
//
// A gpu_texture_output declaring `premultiplied` is drawn with the
// premultiplied "over"; one that does not keeps the straight one.
//
// Every image is read through fixc-opaque-view.fs: the stored rgb on the left
// half, the stored alpha as grey on the right half.
//
// Registration: see the test_gfx_cpu_filter_premultiplied_f2 target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

#include <Gfx/Graph/TexgenNode.hpp>

#include <Crousti/CpuAnalysisNode.hpp>
#include <Crousti/CpuFilterNode.hpp>
#include <Crousti/GfxNode.hpp>
#include <Crousti/ProcessModel.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cstring>
#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct PassThroughCpu
{
  halp_meta(name, "Pass-through CPU filter")
  halp_meta(c_name, "test_pass_through_cpu_f2")
  halp_meta(category, "Test")
  halp_meta(uuid, "6a0e2c3b-1d4f-4e87-9b21-7c5f0a9d3e64")

  struct
  {
    halp::texture_input<"In"> image;
  } inputs;

  struct
  {
    halp::texture_output<"Out"> image;
  } outputs;

  void operator()()
  {
    const auto& in = inputs.image.texture;
    if(!in.bytes || in.width <= 0 || in.height <= 0)
      return;
    auto& out = outputs.image;
    if(out.texture.width != in.width || out.texture.height != in.height)
      out.create(in.width, in.height);
    std::memcpy(out.texture.bytes, in.bytes, std::size_t(in.width) * in.height * 4);
    out.upload();
  }
};

template <bool Premultiplied>
struct TranslucentGpuTexture
{
  halp_meta(name, "Translucent GPU texture")
  halp_meta(c_name, "test_translucent_gpu_texture_f2")
  halp_meta(category, "Test")
  halp_meta(uuid, "c3d51f7e-8a2b-4f06-a9e4-2b7d6c1f0a35")

  struct
  {
  } inputs;

  struct premultiplied_output : halp::gpu_texture_output<"Texture">
  {
    halp_flag(premultiplied);
  };
  struct
  {
    std::conditional_t<
        Premultiplied, premultiplied_output, halp::gpu_texture_output<"Texture">>
        texture;
  } outputs;

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) { }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge*)
  {
    if(m_tex)
      return;
    m_tex = renderer.state.rhi->newTexture(QRhiTexture::RGBA8, QSize{4, 4});
    m_tex->create();
    std::vector<uint8_t> px(4 * 4 * 4);
    for(std::size_t i = 0; i < px.size(); i += 4)
    {
      px[i] = Premultiplied ? 64 : 128;
      px[i + 3] = 128;
    }
    res.uploadTexture(
        m_tex, QRhiTextureUploadEntry{
                   0, 0, QRhiTextureSubresourceUploadDescription{
                             px.data(), quint32(px.size())}});
    outputs.texture.texture.handle = m_tex;
    outputs.texture.texture.width = 4;
    outputs.texture.texture.height = 4;
    outputs.texture.texture.format = halp::gpu_texture::RGBA8;
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

struct HalfRedTexgen : score::gfx::TexgenNode
{
  HalfRedTexgen()
  {
    function = +[](unsigned char* rgba, int w, int h, int) {
      for(int i = 0; i < w * h; i++)
      {
        rgba[4 * i + 0] = 128;
        rgba[4 * i + 1] = 0;
        rgba[4 * i + 2] = 0;
        rgba[4 * i + 3] = 128;
      }
    };
  }
};

QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

struct Shot
{
  bool skipped{};
  std::string error;
  ReadbackImage image;
};

template <typename Build>
Shot render(score::gfx::GraphicsApi api, Build&& build)
{
  Shot s;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      s.error = "no document";
      return;
    }
    GfxPipeline p;
    const int producer = build(p, doc->context(), models);
    const int view = p.addIsf(corpus("fixc-opaque-view.fs"));
    if(producer < 0 || view < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeImageOut(producer, 0), p.imageIn(view, 0));
    const int sink = p.addSink({64, 64});
    p.wire(p.imageOut(view, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(6);
    s.image = p.readback(sink);
    if(!s.image.valid())
      s.error = "empty readback";
  });
  return s;
}

template <typename T>
int addHalp(
    GfxPipeline& p, const score::DocumentContext& ctx,
    std::vector<std::unique_ptr<Process::ProcessModel>>& models)
{
  const int id = int(models.size()) + 1;
  auto model = std::make_unique<oscr::ProcessModel<T>>(
      TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{id}, ctx, nullptr);
  auto* raw = model.get();
  models.push_back(std::move(model));
  return p.addNode(std::unique_ptr<score::gfx::Node>{
      new oscr::GfxNode<T>{*raw, {}, Gfx::exec_controls{}, id, ctx}});
}

void checkStored(const Shot& s, std::array<uint8_t, 4> rgb, uint8_t alpha)
{
  const auto c = s.image.at(16, 32);
  const auto a = s.image.at(48, 32);
  INFO("stored rgb = " << int(c[0]) << "," << int(c[1]) << "," << int(c[2]));
  INFO("stored alpha = " << int(a[0]));
  CHECK(near(c, rgb, 3));
  CHECK(near(a, {alpha, alpha, alpha, 255}, 3));
}

#define F2_REQUIRE_LIVE(s)            \
  if((s).skipped)                     \
    SKIP("backend unavailable");      \
  INFO("error=" << (s).error);        \
  REQUIRE((s).error.empty());         \
  REQUIRE((s).image.valid())
}

TEST_CASE(
    "a pass-through CPU filter keeps a translucent texel",
    "[gfx][avnd][alpha][f2]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Shot direct = render(
      api, [](GfxPipeline& p, const score::DocumentContext&, auto&) {
        return p.addNode(std::make_unique<HalfRedTexgen>());
      });
  F2_REQUIRE_LIVE(direct);
  checkStored(direct, {64, 0, 0, 255}, 128);

  const Shot filtered = render(
      api, [](GfxPipeline& p, const score::DocumentContext& ctx, auto& models) {
        const int tg = p.addNode(std::make_unique<HalfRedTexgen>());
        const int flt = addHalp<PassThroughCpu>(p, ctx, models);
        if(tg < 0 || flt < 0)
          return -1;
        p.wire(p.nodeImageOut(tg, 0), p.node(flt)->input[0]);
        return flt;
      });
  F2_REQUIRE_LIVE(filtered);
  checkStored(filtered, {64, 0, 0, 255}, 128);
}

TEST_CASE(
    "a gpu_texture_output is composited as it declares its alpha",
    "[gfx][avnd][alpha][f2]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const Shot straight = render(
      api, [](GfxPipeline& p, const score::DocumentContext& ctx, auto& models) {
        return addHalp<TranslucentGpuTexture<false>>(p, ctx, models);
      });
  F2_REQUIRE_LIVE(straight);
  checkStored(straight, {64, 0, 0, 255}, 128);

  const Shot premultiplied = render(
      api, [](GfxPipeline& p, const score::DocumentContext& ctx, auto& models) {
        return addHalp<TranslucentGpuTexture<true>>(p, ctx, models);
      });
  F2_REQUIRE_LIVE(premultiplied);
  checkStored(premultiplied, {64, 0, 0, 255}, 128);
}
