// Agent F1: $COUNT_<name> / $BYTESIZE_<name> in a raw raster's integer
// expressions resolve from the buffer bound to an INPUTS storage_input or
// uniform_input, not only from AUXILIARY entries.
//
// INPUTS buffers live in the node's GraphicsStorageResources, which the size
// lookup never searched, so the count read 0 bytes and the MANUAL invocation
// count evaluated to 1. Both producers publish 384 bytes: 24 vec4 elements for
// the storage_input, and 384 / 16 = 24 for the uniform_input's $BYTESIZE. Each
// invocation redraws the target with red = (PASSINDEX + 1) / 255, so the red
// left on screen is the count.
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

constexpr int kUboSize = 384;

struct UboProducerNode final : score::gfx::ProcessNode
{
  UboProducerNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Buffer, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct UboProducerRenderer final : score::gfx::NodeRenderer
{
  QRhiBuffer* ubo{};
  explicit UboProducerRenderer(const UboProducerNode& n)
      : NodeRenderer{n}
  {
  }
  score::gfx::BufferView bufferForOutput(const score::gfx::Port&) override
  {
    return {.handle = ubo, .byte_offset = 0, .byte_size = ubo ? kUboSize : 0};
  }
  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    ubo = renderer.state.rhi->newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, kUboSize);
    ubo->setName("f1.count-ubo");
    ubo->create();
    const std::vector<char> zero(kUboSize, 0);
    res.updateDynamicBuffer(ubo, 0, kUboSize, zero.data());
    m_initialized = true;
  }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override { }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList& renderer) override
  {
    renderer.releaseBuffer(ubo);
    ubo = nullptr;
    m_initialized = false;
  }
};

score::gfx::NodeRenderer* UboProducerNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new UboProducerRenderer{*this};
}
}

TEST_CASE(
    "F1: a raw raster's $COUNT_ / $BYTESIZE_ resolve from its INPUTS buffers",
    "[gfx][raw_raster][expression][f1]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const bool uniform = GENERATE(false, true);
  CAPTURE(backend_name(be), uniform);

  bool built = false;
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> centre{};

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    score::gfx::Port* out{};
    int consumer = -1;
    if(uniform)
    {
      const int producer = p.addNode(std::make_unique<UboProducerNode>());
      consumer = p.addRaster(
          corpus("f1-rr-uniform-count.vs"), corpus("f1-rr-uniform-count.fs"));
      if(producer >= 0)
        out = p.nodeBufferOut(producer, 0);
    }
    else
    {
      const int producer = p.addCsf(corpus("f1-storage-items.cs"));
      consumer = p.addRaster(
          corpus("f1-rr-input-count.vs"), corpus("f1-rr-input-count.fs"));
      if(producer >= 0)
        out = p.bufferOut(producer, 0);
    }
    if(consumer < 0 || !out || !p.bufferIn(consumer, 0))
    {
      err = "node build failed: " + p.error();
      return;
    }
    p.wire(out, p.bufferIn(consumer, 0));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      skipped = p.skipped();
      err = skipped ? std::string{} : p.error();
      return;
    }
    built = true;
    p.render(5);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      err = "readback failed";
      return;
    }
    centre = img.at(img.width / 2, img.height / 2);
  });

  if(skipped)
    SKIP("backend unavailable");
  if(!uniform)
    if(const char* why = compute_shader_skip_reason(be))
      SKIP(why);

  INFO("error=" << err);
  INFO(
      "centre=(" << int(centre[0]) << "," << int(centre[1]) << ","
                 << int(centre[2]) << ")");
  REQUIRE(err.empty());
  REQUIRE(built);
  CHECK(int(centre[0]) == 24);
}
