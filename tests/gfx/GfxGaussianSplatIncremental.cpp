// The Splat renderer (GaussianSplatNode) must survive the incremental graph
// path the app uses, and draw.
//
// GfxContext builds the document's render lists incrementally: a renderer that
// joins a render list through Graph::reconcileAllRenderLists gets initState()
// and addOutputPass(), never init(). GaussianSplatRenderer only implemented
// init(), so the base GenericNodeRenderer::initState ran instead, its uniform
// buffer stayed null, and the first update() queued updateDynamicBuffer(null):
// SIGSEGV in QRhiVulkan::enqueueResourceUpdates from RenderList::renderImpl
// (N84, "Splat loader -> Splat -> Window").
//
// A test node feeds one 256-byte raw splat (the Splat loader layout) to the
// Splat node; the Splat -> passthrough ISF edge is added through the
// incremental path. The splat is red and sits in front of the camera, so the
// centre is red. The ISF sits between Splat and the sink because the node
// drawing straight into the output gets an opaque clear, and the Splat's
// front-to-back "under" blend draws nothing over an opaque destination.
//
// Registration: see the test_gfx_gaussian_splat_incremental target.
#include <score_test/Gfx.hpp>

#include <Threedim/Splat/GaussianSplatNode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <cmath>

using namespace score::test;
using namespace score::test::gfx;

namespace
{
struct RawSplatSource final : score::gfx::Node
{
  RawSplatSource()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Buffer, {}});
  }

  struct Renderer final : score::gfx::NodeRenderer
  {
    using NodeRenderer::NodeRenderer;
    QRhiBuffer* buffer{};

    void initState(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
    {
      if(buffer)
        return;
      float splat[64]{};
      splat[6] = (1.f - 0.5f) / 0.28209479177387814f;
      splat[7] = -0.5f / 0.28209479177387814f;
      splat[8] = -0.5f / 0.28209479177387814f;
      splat[54] = 8.f;
      splat[55] = splat[56] = splat[57] = std::log(0.4f);
      splat[58] = 1.f;
      buffer = r.state.rhi->newBuffer(
          QRhiBuffer::Immutable,
          QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer, sizeof(splat));
      buffer->create();
      res.uploadStaticBuffer(buffer, 0, sizeof(splat), splat);
    }
    void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
    {
      initState(r, res);
    }
    void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*)
        override
    {
    }
    void releaseState(score::gfx::RenderList& r) override
    {
      delete buffer;
      buffer = nullptr;
    }
    void release(score::gfx::RenderList& r) override { releaseState(r); }
    void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
    score::gfx::BufferView bufferForOutput(const score::gfx::Port&) override
    {
      return {.handle = buffer, .byte_offset = 0, .byte_size = buffer ? 256 : 0};
    }
  };

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override
  {
    return new Renderer{*this};
  }
};

struct Shot
{
  bool skipped{};
  std::string error;
  ReadbackImage img;
};
}

TEST_CASE(
    "Splat renders when it joins the render list incrementally",
    "[gfx][threedim][splat][incremental]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  Shot s;
  run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int src = p.addNode(std::make_unique<RawSplatSource>());
    auto splat = std::make_unique<score::gfx::GaussianSplatNode>();
    splat->position = {0.f, 0.f, 3.f};
    splat->center = {0.f, 0.f, 0.f};
    splat->fov = 60.f;
    splat->near = 0.01f;
    splat->far = 100.f;
    auto* splatNode = splat.get();
    const int display = p.addNode(std::move(splat));
    const int pass = p.addIsf(
        QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-passthrough-plain.fs"));
    const int sink = p.addSink({96, 96});
    if(src < 0 || display < 0 || pass < 0)
    {
      s.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeBufferOut(src, 0), splatNode->input[0]);
    p.wire(p.imageOut(pass, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      s.skipped = p.skipped();
      s.error = s.skipped ? std::string{} : p.error();
      return;
    }
    p.render(2);
    p.addEdgeIncremental(p.nodeImageOut(display, 0), p.imageIn(pass, 0));
    p.render(6);
    s.img = p.readback(sink);
    if(!s.img.valid())
      s.error = "empty readback";
  });
  if(s.skipped)
    SKIP("backend unavailable");
  INFO("error=" << s.error);
  REQUIRE(s.error.empty());

  const auto c = s.img.center();
  INFO("centre " << int(c[0]) << " " << int(c[1]) << " " << int(c[2]) << " " << int(c[3]));
  CHECK(c[0] > 128);
  CHECK(c[1] < 64);
  CHECK(c[2] < 64);
}
