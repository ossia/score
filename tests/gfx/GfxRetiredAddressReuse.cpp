// =============================================================================
// A buffer created at an address the allocator recycled is not "retiring".
//
// RenderList keeps a process-wide registry of retired buffers, keyed by
// address, for a few frames after their release: a consumer that still holds
// the handle stops binding it (CustomMesh::drawSingleMesh skips the draw).
// The registry outlived the object, so a new buffer the allocator placed at a
// freed address read as retiring until the entry aged out, five frames. Every
// draw of it was dropped meanwhile: the scene presets rendered nothing for
// their first five frames whenever the mesh buffers landed on such an address
// (test_gfx_library_presets_render, one run in twenty on Vulkan, most runs on
// Metal). Only CSF and MergeGeometries told the registry about the buffers
// they created.
//
// The renderer here does what a producer reallocating its buffer does: every
// frame it creates a buffer and releases it through RenderList::releaseBuffer.
//
// Registration:
//   score_add_gfx_test(retired_address_reuse GfxRetiredAddressReuse.cpp)
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;

namespace
{
struct Churn
{
  int created{};
  int reportedRetiring{};
};

struct ChurnNode final : score::gfx::ProcessNode
{
  Churn* churn{};
  ChurnNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }
  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct ChurnRenderer final : score::gfx::NodeRenderer
{
  const ChurnNode& self;
  explicit ChurnRenderer(const ChurnNode& n)
      : NodeRenderer{n}
      , self{n}
  {
  }
  void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
  {
    m_initialized = true;
  }
  void update(score::gfx::RenderList& r, QRhiResourceUpdateBatch&, score::gfx::Edge*)
      override
  {
    auto* buf = r.state.rhi->newBuffer(
        QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 256);
    REQUIRE(buf->create());
    ++self.churn->created;
    if(score::gfx::RenderList::isRetiringBuffer(buf))
      ++self.churn->reportedRetiring;
    r.releaseBuffer(buf);
  }
  QRhiTexture* textureForOutput(const score::gfx::Port&) override
  {
    return nullptr;
  }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&)
      override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override { m_initialized = false; }
};

score::gfx::NodeRenderer*
ChurnNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new ChurnRenderer{*this};
}
}

TEST_CASE(
    "a buffer created at a recycled address is not reported as retiring",
    "[gfx][lifetime]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  Churn churn;
  bool skipped = false;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    auto node = std::make_unique<ChurnNode>();
    node->churn = &churn;
    const int n = p.addNode(std::move(node));
    const int sink = p.addSink({16, 16});
    REQUIRE(n >= 0);
    REQUIRE(sink >= 0);
    p.wire(p.nodeImageOut(n, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      if(!skipped)
        FAIL(p.error());
      return;
    }
    p.render(30);
  });
  if(skipped)
    SKIP("backend unavailable");
  INFO(churn.reportedRetiring << " of " << churn.created << " new buffers read as retiring");
  REQUIRE(churn.created >= 20);
  CHECK(churn.reportedRetiring == 0);
}
