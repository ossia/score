// =============================================================================
// A buffer a renderer retires while no frame is being recorded
// (RenderList::releaseBuffer from init(), e.g. a CSF regrowing a storage
// buffer to an upstream auxiliary's size) stays alive until the initial batch
// that may still name it has been submitted.
//
// QRhiResource::deleteLater() deletes at once outside a frame, so the initial
// batch recorded during createRenderList could hold uploads to a freed buffer,
// which Metal's enqueueResourceUpdates dereferences. The check reads QRhi's
// own registry of live resources for every buffer the pending batch names, so
// it fails on every backend without dereferencing a freed object. OpenGL keeps uniform
// buffers in CPU memory and never registers them, so dynamic updates are not
// checked there.
// =============================================================================
#include <score_test/Gfx.hpp>

#include "GfxAuxRangeProducer.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include "GfxRhiLiveResources.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
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

// An image inlet and outlet, and a uniform buffer created and uploaded in
// initState() then deleted outright in releaseState(), as the ISF, CSF and raw
// raster renderers do with their material UBO. Before deleting it, releaseState()
// asks the batch the upload went into whether that upload is still pending:
// QRhi frees a batch's ops once a command buffer has taken them, and the batch
// object itself lives in QRhi's pool, so this reads no freed memory.
struct UploadProbe
{
  int inits = 0;
  int releases = 0;
  int releasedWhileQueued = 0;
};

struct UploadProbeNode final : score::gfx::ProcessNode
{
  explicit UploadProbeNode(UploadProbe& probe)
      : probe{probe}
  {
    input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
  }
  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const noexcept override;
  UploadProbe& probe;
};

struct UploadProbeRenderer final : score::gfx::NodeRenderer
{
  explicit UploadProbeRenderer(const UploadProbeNode& n)
      : NodeRenderer{n}
      , probe{n.probe}
  {
  }

  void initState(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    m_ubo = renderer.state.rhi->newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16);
    m_ubo->setName("upload-probe.material");
    m_ubo->create();
    const float zero[4]{};
    res.updateDynamicBuffer(m_ubo, 0, sizeof(zero), zero);
    m_queuedIn = &res;
    ++probe.inits;
  }

  void releaseState(score::gfx::RenderList&) override
  {
    if(!m_ubo)
      return;
    auto* d = QRhiResourceUpdateBatchPrivate::get(m_queuedIn);
    for(int i = 0; i < d->activeBufferOpCount; i++)
      if(d->bufferOps[i].buf == m_ubo)
      {
        ++probe.releasedWhileQueued;
        break;
      }
    delete m_ubo;
    m_ubo = nullptr;
    m_queuedIn = nullptr;
    ++probe.releases;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    initState(renderer, res);
    m_initialized = true;
  }
  void update(score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override { }
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override { }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList& renderer) override
  {
    releaseState(renderer);
    m_initialized = false;
  }

  UploadProbe& probe;
  QRhiBuffer* m_ubo{};
  QRhiResourceUpdateBatch* m_queuedIn{};
};

score::gfx::NodeRenderer*
UploadProbeNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new UploadProbeRenderer{*this};
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
    "buffers released during init stay alive until the initial batch is submitted",
    "[gfx][auxiliary]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool sameBuffer = GENERATE(false, true);
  const std::string consumer
      = GENERATE("csf-aux-count-consumer.cs", "csf-aux-count-toplevel.cs");
  CAPTURE(backend_name(api), sameBuffer, consumer);

  Outcome r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int producer = -1;
    if(sameBuffer)
      producer = p.addNode(std::make_unique<aux_range::AuxProducerNode>());
    else
      producer = p.addCsf(corpus("csf-aux-count-producer.cs"));
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
    "a buffer a renderer releases in init() outlives the initial batch",
    "[gfx]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  Outcome r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int producer = p.addNode(std::make_unique<ReleasingNode>());
    const int cons = p.addCsf(corpus("csf-aux-count-consumer.cs"));
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

// The first frame after createRenderList used to carry the initial batch, with
// every renderer's init()-time uploads, into its first pass. An inlet-spec
// change pending on that frame re-inits the renderer through the surgical
// render-target update before any pass, and releaseState() deleted the buffers
// the batch still named: on OpenGL enqueueResourceUpdates then copied into the
// freed buffer's CPU store (test_gfx_input_sampler_filter, raw raster: a crash
// or a black frame depending on what reused the allocation).
TEST_CASE(
    "a renderer re-initialised on the first frame does not free a buffer the "
    "initial batch still names",
    "[gfx][incremental]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));

  UploadProbe probe;
  bool skipped = false;
  std::string error;
  int initsAfterCreate = 0;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int n = p.addNode(std::make_unique<UploadProbeNode>(probe));
    const int sink = p.addSink({32, 32});
    p.wire(p.nodeImageOut(n, 0), p.sinkInput(sink));
    if(!p.create(api))
    {
      skipped = p.skipped();
      error = skipped ? std::string{} : p.error();
      return;
    }
    initsAfterCreate = probe.inits;

    // A new inlet size before the first frame: that frame takes the surgical
    // path, which releases and re-inits the renderer.
    ossia::render_target_spec spec;
    spec.size = ossia::texture_size{3, 5};
    setRenderTargetSpec(*p.node(n), 0, spec);
    p.render(2);
  });
  if(skipped)
    SKIP("backend unavailable");

  INFO("error=" << error);
  REQUIRE(error.empty());
  INFO(
      "inits after create=" << initsAfterCreate << " inits=" << probe.inits
                            << " releases=" << probe.releases);
  // The first frame did re-init the renderer, so the case below was reached.
  REQUIRE(initsAfterCreate == 1);
  REQUIRE(probe.inits >= 2);
  CHECK(probe.releasedWhileQueued == 0);
}
