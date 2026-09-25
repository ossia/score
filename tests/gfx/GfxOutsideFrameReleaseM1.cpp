// =============================================================================
// Agent M1: a buffer a renderer retires while no frame is being recorded
// (RenderList::releaseBuffer from init(), e.g. a CSF regrowing a storage
// buffer to an upstream auxiliary's size) stays alive until the initial batch
// that may still name it has been submitted.
//
// QRhiResource::deleteLater() deletes at once outside a frame. The initial
// batch recorded during createRenderList then held uploads to a freed buffer,
// which Metal's enqueueResourceUpdates dereferenced and crashed on
// (GfxAuxSizeA5, csf-a5-aux-toplevel.cs). The check reads QRhi's own registry
// of live resources for every buffer the pending batch names, so it fails on
// every backend without dereferencing a freed object. OpenGL keeps uniform
// buffers in CPU memory and never registers them, so dynamic updates are not
// checked there.
// =============================================================================
#include <score_test/Gfx.hpp>

#include "A5AuxProducer.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <QtGui/private/qrhi_p.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QHash<QRhiResource*, bool>& live_resources(QRhiImplementation& rhi);

template <QHash<QRhiResource*, bool> QRhiImplementation::* Member>
struct live_resources_access
{
  friend QHash<QRhiResource*, bool>& live_resources(QRhiImplementation& rhi)
  {
    return rhi.*Member;
  }
};
template struct live_resources_access<&QRhiImplementation::resources>;

QString corpus(const char* file)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR) + QStringLiteral("/") + file;
}

struct ReleasingNode final : score::gfx::ProcessNode
{
  ReleasingNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct ReleasingRenderer final : score::gfx::NodeRenderer
{
  explicit ReleasingRenderer(const ReleasingNode& n)
      : NodeRenderer{n}
  {
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto* buf = renderer.state.rhi->newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, 64);
    buf->setName("m1.released-in-init");
    buf->create();
    const std::vector<char> zero(64, 0);
    res.uploadStaticBuffer(buf, 0, 64, zero.data());
    renderer.releaseBuffer(buf);
    m_initialized = true;
  }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override { }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override { m_initialized = false; }
};

score::gfx::NodeRenderer* ReleasingNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new ReleasingRenderer{*this};
}

struct Outcome
{
  bool skipped = false;
  std::string error;
  int pendingOps = 0;
  std::vector<std::string> dead;
  bool rendered = false;
};

void check_initial_batches(GfxPipeline& p, Outcome& r)
{
  for(const auto& rl : p.graph().renderLists())
  {
    auto* batch = rl ? rl->initialBatch() : nullptr;
    if(!batch)
      continue;
    auto* d = QRhiResourceUpdateBatchPrivate::get(batch);
    const auto& live = live_resources(*d->rhi);
    const bool gl = rl->state.rhi->backend() == QRhi::OpenGLES2;
    for(int i = 0; i < d->activeBufferOpCount; i++)
    {
      const auto& op = d->bufferOps[i];
      if(gl && op.type == QRhiResourceUpdateBatchPrivate::BufferOp::DynamicUpdate)
        continue;
      ++r.pendingOps;
      auto* buf = op.buf;
      if(!live.contains(buf))
        r.dead.push_back(
            "op " + std::to_string(i) + " names a destroyed buffer, retired as \""
            + score::gfx::RenderList::retiredBufferName(buf).toStdString() + "\"");
    }
  }
}
}

TEST_CASE(
    "M1: buffers released during init stay alive until the initial batch is submitted",
    "[gfx][auxiliary][m1]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool sameBuffer = GENERATE(false, true);
  const std::string consumer
      = GENERATE("csf-a5-aux-consumer.cs", "csf-a5-aux-toplevel.cs");
  CAPTURE(backend_name(api), sameBuffer, consumer);

  Outcome r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int producer = -1;
    if(sameBuffer)
      producer = p.addNode(std::make_unique<a5::AuxProducerNode>());
    else
      producer = p.addCsf(corpus("csf-a5-aux-producer.cs"));
    const int cons = p.addCsf(corpus(consumer.c_str()));
    if(producer < 0 || cons < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    p.wire(
        sameBuffer ? p.nodeGeometryOut(producer, 0) : p.geometryOut(producer, 0),
        p.geometryIn(cons, 0));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(cons, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }

    check_initial_batches(p, r);
    if(!r.dead.empty())
      return;

    p.render(2);
    r.rendered = p.readback(sink).valid();
  });
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.pendingOps > 0);
  for(const auto& d : r.dead)
    UNSCOPED_INFO(d);
  CHECK(r.dead.empty());
  CHECK(r.rendered);
}

TEST_CASE(
    "M1: a buffer a renderer releases in init() outlives the initial batch",
    "[gfx][m1]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  Outcome r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addNode(std::make_unique<ReleasingNode>());
    const int cons = p.addCsf(corpus("csf-a5-aux-consumer.cs"));
    if(producer < 0 || cons < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(cons, 0));
    const int sink = p.addSink({32, 32});
    p.wire(p.imageOut(cons, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    check_initial_batches(p, r);
    if(!r.dead.empty())
      return;
    p.render(2);
    r.rendered = p.readback(sink).valid();
  });
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);

  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.pendingOps > 0);
  for(const auto& d : r.dead)
    UNSCOPED_INFO(d);
  CHECK(r.dead.empty());
  CHECK(r.rendered);
}
