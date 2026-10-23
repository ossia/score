// A CSF uniform input fed by a Buffer cable is bound at the range its
// producer publishes, and only to a buffer that covers the block (shader
// board U42, plan steps 3-4, the CSF ingress). The CSF bound the whole
// buffer from byte 0, so a camera published at offset 256 of a larger buffer
// read the wrong bytes, and an undersized one was bound anyway. The producer
// publishes a 512-byte uniform buffer whose first 256 bytes hold 1.0 and the
// next 256 hold 7.0; the CSF writes params.first.x as red.
#include "GfxLogCapture.hpp"
#include "IsfTestCommon.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <QFile>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
struct UboNode final : score::gfx::ProcessNode
{
  quint32 offset{0}, size{0};
  UboNode() { output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Buffer, {}}); }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct UboRenderer final : score::gfx::NodeRenderer
{
  const UboNode& node;
  QRhiBuffer* ubo{};
  explicit UboRenderer(const UboNode& n)
      : NodeRenderer{n}
      , node{n}
  {
  }
  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    ubo = renderer.state.rhi->newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 512);
    ubo->setName("csf_uniform_range.ubo");
    ubo->create();
    std::vector<float> v(128);
    std::fill_n(v.begin(), 64, 1.f);
    std::fill_n(v.begin() + 64, 64, 7.f);
    res.updateDynamicBuffer(ubo, 0, 512, v.data());
    m_initialized = true;
  }
  score::gfx::BufferView bufferForOutput(const score::gfx::Port&) override
  {
    return {.handle = ubo, .byte_offset = node.offset, .byte_size = node.size, .owned = false};
  }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*)
      override
  {
  }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&)
      override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList& r) override
  {
    if(ubo)
      r.releaseBuffer(ubo);
    ubo = nullptr;
    m_initialized = false;
  }
};

score::gfx::NodeRenderer* UboNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new UboRenderer{*this};
}

constexpr auto kCsf = R"_(/*{
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "params", "TYPE": "uniform", "LAYOUT": [
        { "NAME": "first", "TYPE": "vec4" }, { "NAME": "m", "TYPE": "mat4[3]" },
        { "NAME": "b", "TYPE": "vec4" }, { "NAME": "last", "TYPE": "vec4" } ] },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "rgba8", "WIDTH": "4", "HEIGHT": "4" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [4, 4, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
  ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
  if(pos.x >= 4 || pos.y >= 4)
    return;
  imageStore(outputImage, pos, vec4(params.first.x / 255.0, params.last.x / 255.0, 0.0, 1.0));
}
)_";

struct Shot
{
  bool skipped = false;
  std::string err;
  std::array<uint8_t, 4> centre{};
  int warnings = 0;
};

Shot shoot(score::gfx::GraphicsApi api, quint32 offset, quint32 size)
{
  Shot s;
  QTemporaryDir dir;
  const QString cs = dir.filePath("uniform-range.cs");
  {
    QFile f{cs};
    if(!dir.isValid() || !f.open(QIODevice::WriteOnly))
    {
      s.err = "cannot write the shader";
      return s;
    }
    f.write(kCsf);
  }
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    LogCapture log;
    GfxPipeline p;
    auto n = std::make_unique<UboNode>();
    n->offset = offset;
    n->size = size;
    const int prod = p.addNode(std::move(n));
    const int csf = p.addCsf(cs);
    if(prod < 0 || csf < 0)
    {
      s.err = "build failed: " + p.error();
      return;
    }
    const int sink = p.addSink({4, 4});
    p.wire(p.nodeBufferOut(prod, 0), p.bufferIn(csf, 0));
    p.wire(p.imageOut(csf, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.err = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(3);
    const auto img = p.readback(sink);
    if(!img.valid())
    {
      s.err = "readback failed";
      return;
    }
    s.centre = img.center();
    s.warnings = log.count({u"params", u"smaller than"});
  });
  return s;
}
}

TEST_CASE(
    "a CSF uniform input binds the published range of its Buffer cable",
    "[gfx][csf][ubo][U42]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto whole = shoot(api, 0, 0);
  if(whole.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  INFO("error=" << whole.err);
  REQUIRE(whole.err.empty());
  CHECK(int(whole.centre[0]) == 1);

  // The second half of the buffer.
  const auto upper = shoot(api, 256, 256);
  INFO("error=" << upper.err);
  REQUIRE(upper.err.empty());
  INFO("offset 256: first " << int(upper.centre[0]) << ", last " << int(upper.centre[1]));
  CHECK(int(upper.centre[0]) == 7);
  CHECK(int(upper.centre[1]) == 7);

  // 128 published bytes do not cover the 240-byte block: zeros, one warning.
  const auto shortRange = shoot(api, 0, 128);
  INFO("error=" << shortRange.err);
  REQUIRE(shortRange.err.empty());
  CHECK(int(shortRange.centre[0]) == 0);
  CHECK(shortRange.warnings == 1);
}
