// =============================================================================
// Agent F1: every buffer the initial batch names is still alive when that
// batch is submitted, whichever path freed it during init().
//
// QRhiResource::deleteLater() deletes at once outside a frame. M1 parked only
// RenderList::releaseBuffer; two other frees still ran immediately:
//
//  * IsfBindingsBuilder's letGoOf, when bindUpstreamBuffers swaps the owned
//    placeholder ensureStorageResources just allocated and zero-filled for the
//    upstream buffer of a uniform_input / read_only storage_input (ISF simple,
//    persistent, multipass; raw raster). A consumer added to a graph that is
//    already rendering initialises against a producer that already publishes
//    its buffer, so the swap happens outside a frame;
//  * RenderList::dropAdoptedBuffer, when the last consumer of a buffer its
//    owner already released lets go.
//
// On the first build the producer inits after its consumer and the swap
// happens in update(). bindUpstreamBuffers patched only the first SRB it was
// handed: every later one found the entry already pointing at the upstream
// buffer and kept the placeholder, which the swap had just freed. The
// persistent and multipass ISF cases crashed on their second frame.
//
// The check reads QRhi's registry of live resources for every buffer the
// pending batch names, as GfxOutsideFrameReleaseM1 does. OpenGL keeps uniform
// buffers in CPU memory and never registers them, so dynamic updates are not
// checked there.
// =============================================================================
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <QtGui/private/qrhi_p.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <functional>
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
    return {.handle = ubo, .byte_offset = 0, .byte_size = ubo ? ubo->size() : 0};
  }
  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    ubo = renderer.state.rhi->newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 512);
    ubo->setName("f1.upstream-ubo");
    ubo->create();
    const std::vector<char> zero(512, 0);
    res.updateDynamicBuffer(ubo, 0, 512, zero.data());
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

struct OrphanNode final : score::gfx::ProcessNode
{
  OrphanNode()
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override;
};

struct OrphanRenderer final : score::gfx::NodeRenderer
{
  explicit OrphanRenderer(const OrphanNode& n)
      : NodeRenderer{n}
  {
  }
  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto* buf = renderer.state.rhi->newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, 64);
    buf->setName("f1.orphan-dropped-in-init");
    buf->create();
    const std::vector<char> zero(64, 0);
    res.uploadStaticBuffer(buf, 0, 64, zero.data());
    score::gfx::RenderList::adoptBuffer(buf);
    renderer.releaseBuffer(buf);
    score::gfx::RenderList::dropAdoptedBuffer(buf);
    m_initialized = true;
  }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override { }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override { m_initialized = false; }
};

score::gfx::NodeRenderer* OrphanNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new OrphanRenderer{*this};
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

void report(const Outcome& r)
{
  INFO("error=" << r.error);
  REQUIRE(r.error.empty());
  CHECK(r.pendingOps > 0);
  for(const auto& d : r.dead)
    UNSCOPED_INFO(d);
  CHECK(r.dead.empty());
  CHECK(r.rendered);
}

using Step = std::function<bool(GfxPipeline&, int)>;

Outcome run(score::gfx::GraphicsApi api, const Step& build, const Step& addLive = {})
{
  Outcome r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int sink = p.addSink({32, 32});
    if(!build(p, sink))
    {
      r.error = "node build failed: " + p.error();
      return;
    }
    if(!p.create(api))
    {
      r.skipped = p.skipped();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    if(addLive)
    {
      p.render(2);
      if(!addLive(p, sink))
      {
        r.error = "live node build failed: " + p.error();
        return;
      }
    }
    check_initial_batches(p, r);
    if(!r.dead.empty())
      return;
    p.render(2);
    r.rendered = p.readback(sink).valid();
  });
  return r;
}
}

TEST_CASE(
    "F1: an ISF uniform_input placeholder replaced by a live upstream outlives "
    "the pending batch",
    "[gfx][isf][binding][f1]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const std::string shader = GENERATE(
      "binding-uniform-input.fs", "isf-persistent-uniform-input.fs",
      "isf-multipass-uniform-input.fs");
  const bool live = GENERATE(false, true);
  CAPTURE(backend_name(api), shader, live);

  int producer = -1;
  const auto consumer = [&](GfxPipeline& p, int sink, bool incremental) {
    const int node = p.addIsf(corpus(shader.c_str()));
    if(node < 0 || !p.bufferIn(node, 0))
      return false;
    p.wire(p.nodeBufferOut(producer, 0), p.bufferIn(node, 0));
    if(incremental)
      p.addEdgeIncremental(p.imageOut(node, 0), p.sinkInput(sink));
    else
      p.wire(p.imageOut(node, 0), p.sinkInput(sink));
    return true;
  };
  const Step build = [&](GfxPipeline& p, int sink) {
    producer = p.addNode(std::make_unique<UboProducerNode>());
    return producer >= 0 && consumer(p, sink, false);
  };
  const Step addLive = [&](GfxPipeline& p, int sink) { return consumer(p, sink, true); };

  const auto r = run(api, build, live ? addLive : Step{});
  if(r.skipped)
    SKIP("backend unavailable");
  report(r);
}

TEST_CASE(
    "F1: a raw-raster storage_input placeholder replaced by a live upstream "
    "outlives the pending batch",
    "[gfx][raw_raster][binding][f1]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool live = GENERATE(false, true);
  CAPTURE(backend_name(api), live);

  int producer = -1;
  const auto consumer = [&](GfxPipeline& p, int sink, bool incremental) {
    const int node
        = p.addRaster(corpus("rr-storage-input.vs"), corpus("rr-storage-input.fs"));
    if(node < 0 || !p.bufferIn(node, 0))
      return false;
    p.wire(p.bufferOut(producer, 0), p.bufferIn(node, 0));
    if(incremental)
      p.addEdgeIncremental(p.imageOut(node, 0), p.sinkInput(sink));
    else
      p.wire(p.imageOut(node, 0), p.sinkInput(sink));
    return true;
  };
  const Step build = [&](GfxPipeline& p, int sink) {
    producer = p.addCsf(corpus("syn-storage-colour.cs"));
    return producer >= 0 && p.bufferOut(producer, 0) && consumer(p, sink, false);
  };
  const Step addLive = [&](GfxPipeline& p, int sink) { return consumer(p, sink, true); };

  const auto r = run(api, build, live ? addLive : Step{});
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  report(r);
}

TEST_CASE(
    "F1: an orphaned adopted buffer dropped in init outlives the initial batch",
    "[gfx][lifetime][f1]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  const auto r = run(api, [&](GfxPipeline& p, int sink) {
    const int producer = p.addNode(std::make_unique<OrphanNode>());
    const int consumer = p.addCsf(corpus("csf-a5-aux-consumer.cs"));
    if(producer < 0 || consumer < 0)
      return false;
    p.wire(p.nodeGeometryOut(producer, 0), p.geometryIn(consumer, 0));
    p.wire(p.imageOut(consumer, 0), p.sinkInput(sink));
    return true;
  });
  if(r.skipped)
    SKIP("backend unavailable");
  if(const char* why = compute_shader_skip_reason(api))
    SKIP(why);
  report(r);
}
