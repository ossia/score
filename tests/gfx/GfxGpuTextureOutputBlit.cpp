// A CPU node publishing a GPU texture reaches an ordinary image input.
//
// A halp object with a gpu_texture_output hands its own QRhiTexture to the
// graph. A consumer that samples the texture directly (a STATIC input) saw it;
// one that asks the producer to draw into its per-edge target got the default
// blit, which sampled a binding nothing filled: black on Vulkan and OpenGL, a
// missing-sampler abort under Metal validation. The producer here publishes a
// solid red texture and the consumer is a plain passthrough.
//
// Registration: see the test_gfx_gpu_texture_output_blit target.
#include <score_test/Gfx.hpp>
#include <score_test/Document.hpp>

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

#include <vector>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct RedTexture
{
  halp_meta(name, "Red texture")
  halp_meta(c_name, "test_red_texture")
  halp_meta(category, "Test")
  halp_meta(uuid, "4b7f7d0e-3c1a-4f2e-9a55-2d6e8c1b7a10")

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
    m_tex = renderer.state.rhi->newTexture(QRhiTexture::RGBA8, QSize{4, 4});
    m_tex->create();
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

QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}
}

TEST_CASE(
    "a published GPU texture reaches an ordinary image input",
    "[gfx][avnd][texture]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage img;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      err = "no document";
      return;
    }
    const auto& ctx = doc->context();
    auto model = std::make_unique<oscr::ProcessModel<RedTexture>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));

    GfxPipeline p;
    const int producer = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<RedTexture>{*raw, {}, Gfx::exec_controls{}, 1, ctx}});
    const int pass = p.addIsf(corpus("isf-passthrough-plain.fs"));
    if(producer < 0 || pass < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeImageOut(producer, 0), p.imageIn(pass, 0));
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(pass, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(4);
    img = p.readback(sink);
    if(!img.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  const auto px = img.center();
  INFO("centre " << int(px[0]) << " " << int(px[1]) << " " << int(px[2]));
  CHECK(px[0] > 200);
  CHECK(px[1] < 40);
  CHECK(px[2] < 40);
}
