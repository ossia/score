// A CPU node publishing a GPU texture reaches an ordinary image input.
//
// A halp object with a gpu_texture_output hands its own QRhiTexture to the
// graph. A consumer that asks the producer to draw into its per-edge target
// must get that texture through the default blit, not an unfilled binding
// (black on Vulkan and OpenGL, a missing-sampler abort under Metal
// validation). The producer publishes solid red and the consumer is a plain
// passthrough.
//
// A cube, 3D or array texture has no 2D blit: the default pass keeps sampling
// its empty 2D placeholder rather than binding a texture of the wrong type,
// which Metal validation rejects.
#include "IsfTestCommon.hpp"

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
using score::test::gfx::isf::corpus;

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
                   0, 0, QRhiTextureSubresourceUploadDescription(
                             px.data(), px.size())});
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

struct CubeTexture
{
  halp_meta(name, "Cube texture")
  halp_meta(c_name, "test_cube_texture")
  halp_meta(category, "Test")
  halp_meta(uuid, "9d2e61a4-7b3c-4e58-a0f1-6c8b2d4e9f37")

  struct
  {
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Texture"> texture;
  } outputs;

  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) { }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch&, score::gfx::Edge*)
  {
    if(m_tex)
      return;
    m_tex = renderer.state.rhi->newTexture(
        QRhiTexture::RGBA8, QSize{4, 4}, 1, QRhiTexture::CubeMap);
    m_tex->create();
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


// A CPU texture outlet (green) and a GPU texture outlet (red) on one node: a
// cable from each outlet must sample that outlet, whichever kind it is.
struct MixedOutlets
{
  halp_meta(name, "Mixed texture outlets")
  halp_meta(c_name, "test_mixed_texture_outlets")
  halp_meta(category, "Test")
  halp_meta(uuid, "e3b61c0a-58f4-4d27-9a0c-7c2d91f4b6e5")

  struct
  {
  } inputs;

  struct
  {
    halp::texture_output<"Cpu"> cpu;
    halp::gpu_texture_output<"Gpu"> gpu;
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
                   0, 0, QRhiTextureSubresourceUploadDescription(
                             px.data(), px.size())});
    outputs.gpu.texture.handle = m_tex;
    outputs.gpu.texture.width = 4;
    outputs.gpu.texture.height = 4;
    outputs.gpu.texture.format = halp::gpu_texture::RGBA8;
  }

  void release(score::gfx::RenderList&)
  {
    delete m_tex;
    m_tex = nullptr;
    outputs.gpu.texture.handle = nullptr;
  }

  void runInitialPasses(
      score::gfx::RenderList&, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge&)
  {
  }

  void operator()()
  {
    if(!outputs.cpu.texture.bytes)
      outputs.cpu.create(4, 4);
    for(int y = 0; y < 4; y++)
      for(int x = 0; x < 4; x++)
        outputs.cpu.set(x, y, 0, 255, 0, 255);
    outputs.cpu.upload();
  }

  QRhiTexture* m_tex{};
};

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

TEST_CASE(
    "a published cube texture is not blitted through a 2D sampler",
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
    auto model = std::make_unique<oscr::ProcessModel<CubeTexture>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));

    GfxPipeline p;
    const int producer = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<CubeTexture>{*raw, {}, Gfx::exec_controls{}, 1, ctx}});
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
}

TEST_CASE(
    "each outlet of a node with CPU and GPU texture outlets reaches its own cable",
    "[gfx][avnd][texture]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  bool skipped = false;
  std::string err;
  ReadbackImage fromCpu, fromGpu;
  std::vector<std::unique_ptr<Process::ProcessModel>> models;
  run_in_gui_app([&](const score::GUIApplicationContext& app) {
    auto* doc = new_document(app);
    if(!doc)
    {
      err = "no document";
      return;
    }
    const auto& ctx = doc->context();
    auto model = std::make_unique<oscr::ProcessModel<MixedOutlets>>(
        TimeVal::fromMsecs(1000), Id<Process::ProcessModel>{1}, ctx, nullptr);
    auto* raw = model.get();
    models.push_back(std::move(model));

    GfxPipeline p;
    const int producer = p.addNode(std::unique_ptr<score::gfx::Node>{
        new oscr::GfxNode<MixedOutlets>{*raw, {}, Gfx::exec_controls{}, 1, ctx}});
    const int passCpu = p.addIsf(corpus("isf-passthrough-plain.fs"));
    const int passGpu = p.addIsf(corpus("isf-passthrough-plain.fs"));
    if(producer < 0 || passCpu < 0 || passGpu < 0)
    {
      err = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeImageOut(producer, 0), p.imageIn(passCpu, 0));
    p.wire(p.nodeImageOut(producer, 1), p.imageIn(passGpu, 0));
    const int sinkCpu = p.addSink({16, 16});
    const int sinkGpu = p.addSink({16, 16});
    p.wire(p.imageOut(passCpu, 0), p.sinkInput(sinkCpu));
    p.wire(p.imageOut(passGpu, 0), p.sinkInput(sinkGpu));
    if(!p.create(api))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    p.render(6);
    fromCpu = p.readback(sinkCpu);
    fromGpu = p.readback(sinkGpu);
    if(!fromCpu.valid() || !fromGpu.valid())
      err = "empty readback";
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO("error=" << err);
  REQUIRE(err.empty());
  const auto c = fromCpu.center();
  const auto g = fromGpu.center();
  INFO("cpu outlet " << int(c[0]) << " " << int(c[1]) << " " << int(c[2]));
  INFO("gpu outlet " << int(g[0]) << " " << int(g[1]) << " " << int(g[2]));
  CHECK((c[1] > 200 && c[0] < 40));
  CHECK((g[0] > 200 && g[1] < 40));
}
