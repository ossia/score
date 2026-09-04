// =============================================================================
// A raw-Vulkan buffer copy recorded through beginExternal() must be ordered
// against the QRhi staging copies of the resource-update batch that was
// submitted earlier in the SAME command buffer.
//
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_buffer_copy_barrier_sync
//
// THE DEFECT THIS PINS. Vulkan synchronization validation reported, on
// Windows/NVIDIA and reproduced here on Linux/Mesa:
//
//   [ SYNC-HAZARD-WRITE-AFTER-WRITE ] vkCmdCopyBuffer(): Hazard
//   WRITE_AFTER_WRITE for dstBuffer VkBuffer[ScenePreprocessor::inst.translations]
//   ... (usage: SYNC_COPY_TRANSFER_WRITE, prior_usage: SYNC_COPY_TRANSFER_WRITE,
//   write_barriers: 0, command: vkCmdCopyBuffer)
//
// The two conflicting writes are:
//   W1  ScenePreprocessorNode.cpp growBuf() -> RhiClearBuffer::clearBuffer(
//       rhi, res, buf, 0, newCap) -> QRhiResourceUpdateBatch::uploadStaticBuffer.
//       QRhi lowers that to a staging vkCmdCopyBuffer over [0, capacity), and
//       RenderList submits the batch before runInitialPasses.
//   W2  ScenePreprocessorNode::issuePendingGpuCopies' per-instance concat copy
//       into [slot_base*16, (slot_base+count)*16) of the same buffer, recorded
//       raw through beginExternal().
// W1 covers the whole buffer, so it overlaps W2 for ANY n_regular_cmds,
// slot_base and count -- on every frame where growBuf actually (re)allocates.
// (The unrelated pair the growBuf comment worries about -- the regular-range
// identity uploadStaticBuffer at [0, n_regular_cmds*16) versus the copies at
// [slot_base*16, ...) with slot_base >= n_regular_cmds -- genuinely never
// overlaps. The zero-clear is what does.)
//
// Nothing ordered them, because copies recorded through beginExternal() are
// invisible to QRhi's own per-buffer barrier tracking
// (QRhiVulkan::trackedBufferBarrier), and score's batch barrier
// (beginBufferCopyBarrier) named only COMPUTE_SHADER / SHADER_WRITE in its
// source scope -- nothing about a prior TRANSFER write. The same omission
// produced the mirror-image READ_AFTER_WRITE on the SOURCE buffer, which an
// upstream producer had just filled with its own uploadStaticBuffer.
//
// WHAT THE TEST DOES. Two runs of the same pipeline under a stderr capture
// with synchronization validation on. They are separate runs, not two halves
// of one frame, because beginBufferCopyBarrier records a GLOBAL
// VkMemoryBarrier: once one is recorded in a command buffer, every later
// transfer in it is ordered after the update batch as well. Put both halves in
// one frame and each shields the other, and both pass on a broken build.
//
//  1. PROBES, and the reason this test cannot pass vacuously. Two probe buffer
//     pairs get the exact W1 shape -- an uploadStaticBuffer over their whole
//     extent from update()'s batch -- and are then copied into from
//     runInitialPasses. The BARE pair goes first, with no bracket and
//     BufferCopyBarrier::None, i.e. no synchronisation whatsoever: it MUST be
//     reported, and if it is not, the layer or the capture is not doing its
//     job and the test SKIPs with that reason rather than going green on
//     silence. The BARRIERED pair follows, same copy inside a
//     begin/endBufferCopyBarrier bracket, and must be clean.
//
//  2. THE ENGINE PATH, with the probes switched off so no barrier of the
//     test's own can shield it: zero hazards naming a ScenePreprocessor::inst.*
//     buffer, and zero naming the source transforms buffer, while a real
//     ScenePreprocessorNode consumes a real instance_component whose
//     instance_transforms is a GPU buffer -- the path that allocates
//     inst.attribs (named inst.translations / inst.colors when the hazard
//     above was captured, before the two were interleaved into one binding)
//     and then GPU-copies into it.
//
// MEASURED, on this machine (Mesa, Vulkan, SCORE_GPU_VALIDATION=2), against
// tests/gfx/GfxInstancerShrink.cpp's scenario which drives the same engine
// path: 218 SYNC-HAZARD lines before the fix (100 WRITE_AFTER_WRITE on
// ScenePreprocessor::inst.translations + 110 READ_AFTER_WRITE on the source
// transforms buffers + 8 unrelated image-view ones), 8 after -- i.e. every
// buffer hazard gone, the unrelated image-view ones untouched.
//
// NEGATIVE CONTROL (one line, for the orchestrator): in
// src/plugins/score-plugin-gfx/Gfx/Graph/RhiComputeBarrier.cpp, drop
// VK_PIPELINE_STAGE_TRANSFER_BIT from kCopySrcStages (or
// VK_ACCESS_TRANSFER_WRITE_BIT from kCopySrcAccess). Run 2 goes red on both
// counts and run 1 goes red on the barriered probe; the bare probe is
// unaffected, since it never had a barrier to weaken.
//
// NOT ESTABLISHED: whether this hazard has anything to do with the
// VK_ERROR_DEVICE_LOST seen under repeated fullscreen toggling. A transfer
// write-after-write yields wrong bytes, not a lost device. This test pins the
// hazard on its own merits.
// =============================================================================
#include <score_test/Gfx.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>

#include <score/gfx/Vulkan.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#if QT_HAS_VULKAN
#include <QVulkanInstance>
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#define GFX_SYNC_HAVE_STDERR_CAPTURE 1
#else
#define GFX_SYNC_HAVE_STDERR_CAPTURE 0
#endif

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

constexpr int kSize = 64;
constexpr int kInstances = 32;    // instance-group size -> inst.* buffer extent
constexpr int kProbeBytes = 1024; // probe buffer size, 16 B-aligned
constexpr float kQuadW = 0.0625f;

// Both env gates must be decided before the process' static QVulkanInstance is
// created, i.e. before any test body runs. Set-if-unset so an explicit caller
// setting wins. Level 2 is what turns on the layer's synchronization checks
// (score/gfx/Vulkan.cpp: VK_LAYER_VALIDATE_SYNC).
const bool g_env_init = [] {
  const auto setDefault = [](const char* name, const char* value) {
    if(!qEnvironmentVariableIsSet(name))
      qputenv(name, value);
  };
  setDefault("SCORE_GPU_VALIDATION", "2");
  setDefault("QT_FORCE_STDERR_LOGGING", "1");
  // The engine's own [BUFTRACE] chatter would only dilute the capture.
  setDefault("SCORE_BUFTRACE", "0");
  return true;
}();

#if GFX_SYNC_HAVE_STDERR_CAPTURE
/// Captures fd 2 between construction and finish(). QVulkanInstance routes the
/// validation layer's messages through the Qt message handler, which lands
/// there. Same shape as tests/gfx/GfxRenderPassLeak.cpp, pipe enlarged so a
/// torrent of validation output cannot block the writer.
struct StderrCapture
{
  int saved{-1};
  int fds[2]{-1, -1};

  StderrCapture()
  {
    ::fflush(stderr);
    saved = ::dup(2);
    [[maybe_unused]] const int r = ::pipe(fds);
#if defined(F_SETPIPE_SZ)
    ::fcntl(fds[1], F_SETPIPE_SZ, 1 << 22);
#endif
    ::dup2(fds[1], 2);
    ::close(fds[1]);
  }

  std::string finish()
  {
    ::fflush(stderr);
    ::dup2(saved, 2);
    ::close(saved);
    std::string r;
    char buf[4096];
    ssize_t n;
    while((n = ::read(fds[0], buf, sizeof buf)) > 0)
      r.append(buf, std::size_t(n));
    ::close(fds[0]);
    return r;
  }
};
#endif

/// Lines that are a synchronization-validation hazard mentioning @p needle.
/// Counted per line, because the layer emits one line per conflicting copy
/// region.
int hazardLines(const std::string& log, const char* needle)
{
  int n = 0;
  std::size_t start = 0;
  while(start < log.size())
  {
    std::size_t end = log.find('\n', start);
    if(end == std::string::npos)
      end = log.size();
    const std::string_view line{log.data() + start, end - start};
    if(line.find("SYNC-HAZARD") != std::string_view::npos
       && line.find(needle) != std::string_view::npos)
      ++n;
    start = end + 1;
  }
  return n;
}

// --- The scene the ScenePreprocessorNode consumes ---------------------------
//
// One instance_component: a CPU-backed 2 px quad prototype, kInstances
// instances, and a GPU-resident mat4 transforms buffer. That is the minimum
// that makes rebuildMDI allocate inst.attribs through
// growBuf (which zero-clears the whole new capacity through the update batch)
// and then queue the per-instance GPU copies into them.

void writeMat4(float* out, float tx, float ty)
{
  std::memset(out, 0, 16 * sizeof(float));
  out[0] = out[5] = out[10] = out[15] = 1.f;
  out[12] = tx;
  out[13] = ty;
}

std::shared_ptr<const ossia::mesh_component> makePrototypeMesh()
{
  auto positions = std::make_shared<std::vector<float>>(std::vector<float>{
      0.f,    -1.f, 0.f, //
      kQuadW, -1.f, 0.f, //
      kQuadW, 1.f,  0.f, //
      0.f,    -1.f, 0.f, //
      kQuadW, 1.f,  0.f, //
      0.f,    1.f,  0.f, //
  });
  auto indices = std::make_shared<std::vector<uint32_t>>(
      std::vector<uint32_t>{0, 1, 2, 3, 4, 5});

  auto posRes = std::make_shared<ossia::buffer_resource>();
  {
    ossia::buffer_data bd;
    bd.data = std::shared_ptr<const void>(positions, positions->data());
    bd.byte_size = int64_t(positions->size() * sizeof(float));
    bd.usage_hint = ossia::buffer_data::usage::vertex_buffer;
    posRes->resource = bd;
    posRes->dirty_index = 1;
  }
  auto idxRes = std::make_shared<ossia::buffer_resource>();
  {
    ossia::buffer_data bd;
    bd.data = std::shared_ptr<const void>(indices, indices->data());
    bd.byte_size = int64_t(indices->size() * sizeof(uint32_t));
    bd.usage_hint = ossia::buffer_data::usage::index_buffer;
    idxRes->resource = bd;
    idxRes->dirty_index = 1;
  }

  ossia::mesh_primitive prim;
  prim.vertex_buffers.push_back(posRes);
  prim.index_buffer = idxRes;
  {
    ossia::vertex_attribute a;
    a.semantic = ossia::attribute_semantic::position;
    a.format = ossia::vertex_format::float3;
    a.buffer_index = 0;
    a.byte_offset = 0;
    a.byte_stride = 12;
    a.rate = ossia::vertex_attribute::input_rate::per_vertex;
    prim.attributes.push_back(a);
  }
  prim.topology = ossia::primitive_topology::triangles;
  prim.index_type = ossia::index_format::uint32;
  prim.vertex_count = 6;
  prim.index_count = 6;
  prim.bounds = ossia::compute_aabb_from_positions(positions->data(), 6);
  prim.stable_id = 0x5C0EC5D6u;

  auto mesh = std::make_shared<ossia::mesh_component>();
  mesh->primitives.push_back(std::move(prim));
  mesh->bounds = mesh->primitives[0].bounds;
  mesh->dirty_index = 1;
  return mesh;
}

// --- The harness node --------------------------------------------------------

struct HazardProbeNode final : score::gfx::ProcessNode
{
  std::shared_ptr<const ossia::mesh_component> proto = makePrototypeMesh();

  // The two halves run as SEPARATE pipelines, because beginBufferCopyBarrier
  // records a GLOBAL VkMemoryBarrier: one recorded anywhere in a command
  // buffer orders every later transfer in it. Issuing the probes in the same
  // frame as the ScenePreprocessor's own copies would therefore have each half
  // shielding the other, and both would pass on a broken build.
  bool issueProbes = false;

  explicit HazardProbeNode(bool probes)
      : issueProbes{probes}
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }
  ~HazardProbeNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct HazardProbeRenderer final : score::gfx::NodeRenderer
{
  HazardProbeNode& self;
  ossia::scene_spec m_scene;

  QRhiBuffer* m_transforms{};
  // Probe pair 1: copied inside a begin/endBufferCopyBarrier bracket.
  QRhiBuffer* m_probeBarrieredSrc{};
  QRhiBuffer* m_probeBarrieredDst{};
  // Probe pair 2: copied with NO barrier at all -- the positive control.
  QRhiBuffer* m_probeBareSrc{};
  QRhiBuffer* m_probeBareDst{};

  int m_probesIssued{0};

  explicit HazardProbeRenderer(const HazardProbeNode& n)
      : NodeRenderer{n}
      , self{const_cast<HazardProbeNode&>(n)}
  {
  }

  QRhiBuffer* makeBuf(QRhi& rhi, int bytes, const char* name)
  {
    auto* b = rhi.newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::UsageFlags(
            QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer),
        bytes);
    b->setName(name);
    b->create();
    return b;
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *r.state.rhi;

    m_transforms
        = makeBuf(rhi, kInstances * 64, "BufferCopyBarrierTest::transforms");
    {
      std::vector<float> mats(kInstances * 16);
      for(int i = 0; i < kInstances; ++i)
        writeMat4(mats.data() + i * 16, -1.f + 2.f * float(i) / kInstances, 0.f);
      res.uploadStaticBuffer(
          m_transforms, 0, quint32(mats.size() * sizeof(float)), mats.data());
    }

    m_probeBarrieredSrc
        = makeBuf(rhi, kProbeBytes, "BufferCopyBarrierTest::barriered_src");
    m_probeBarrieredDst
        = makeBuf(rhi, kProbeBytes, "BufferCopyBarrierTest::barriered_dst");
    m_probeBareSrc
        = makeBuf(rhi, kProbeBytes, "BufferCopyBarrierTest::bare_src");
    m_probeBareDst
        = makeBuf(rhi, kProbeBytes, "BufferCopyBarrierTest::bare_dst");

    // Publish the instance group.
    auto inst = std::make_shared<ossia::instance_component>();
    {
      auto tr = std::make_shared<ossia::buffer_resource>();
      ossia::gpu_buffer_handle h;
      h.native_handle = m_transforms;
      h.byte_offset = 0;
      h.byte_size = kInstances * 64;
      tr->resource = h;
      tr->dirty_index = 1;

      inst->prototype = self.proto;
      inst->instance_transforms = std::move(tr);
      inst->instance_count = kInstances;
      inst->transform_type
          = ossia::instance_component::transform_format::mat4;
      inst->dirty_index = 1;
    }

    auto children = std::make_shared<std::vector<ossia::scene_payload>>();
    children->push_back(ossia::instance_component_ptr(std::move(inst)));
    auto root = std::make_shared<ossia::scene_node>();
    root->children = std::move(children);
    auto roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
    roots->push_back(std::move(root));

    auto st = std::make_shared<ossia::scene_state>();
    st->roots = std::move(roots);
    st->version = 1;
    st->dirty_index = 1;
    m_scene.state = std::move(st);

    m_initialized = true;
  }

  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch& res,
      score::gfx::Edge*) override
  {
    if(!self.issueProbes || m_probesIssued > 0)
      return;

    // Exactly growBuf's shape: a full-extent uploadStaticBuffer into the batch
    // RenderList submits BEFORE runInitialPasses runs. On Vulkan this is a
    // staging vkCmdCopyBuffer -- a TRANSFER write over [0, kProbeBytes).
    const std::vector<char> zeros(kProbeBytes, 0);
    for(auto* b : {m_probeBarrieredSrc, m_probeBarrieredDst, m_probeBareSrc,
                   m_probeBareDst})
      res.uploadStaticBuffer(b, 0, kProbeBytes, zeros.data());
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(self.issueProbes && m_probesIssued == 0)
    {
      auto& rhi = *renderer.state.rhi;

      // ORDER MATTERS, and the reason is the whole point of the fix.
      // beginBufferCopyBarrier records a GLOBAL VkMemoryBarrier, so once one
      // has been recorded in this command buffer every later transfer in it is
      // ordered after the update batch too. The bare control must therefore
      // come FIRST, while nothing yet stands between it and the batch's
      // uploads -- otherwise it is protected by the very barrier it exists to
      // prove is needed, and the test would skip itself on a build that is
      // actually broken.

      // BARE positive control: no bracket, barriers explicitly declined. This
      // one MUST be reported; it is how the test knows the layer's sync checks
      // are on and the capture is wired.
      cb.beginExternal();
      score::gfx::copyBuffer(
          rhi, cb, m_probeBareSrc, m_probeBareDst, kProbeBytes, 0, 0,
          score::gfx::BufferCopyBarrier::None);
      cb.endExternal();

      // BARRIERED: what the engine does. The bracket's source scope must cover
      // the TRANSFER writes the update batch above just performed.
      cb.beginExternal();
      score::gfx::beginBufferCopyBarrier(rhi, cb);
      score::gfx::copyBuffer(
          rhi, cb, m_probeBarrieredSrc, m_probeBarrieredDst, kProbeBytes, 0, 0,
          score::gfx::BufferCopyBarrier::None);
      score::gfx::endBufferCopyBarrier(rhi, cb);
      cb.endExternal();

      m_probesIssued = 1;
    }

    // Publish the scene downstream, as every scene producer does.
    if(!m_scene.state)
      return;
    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it
        = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    rn_it->second->process(
        int(it - sink->node->input.begin()), m_scene, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }

  void release(score::gfx::RenderList&) override
  {
    for(auto** b : {&m_transforms, &m_probeBarrieredSrc, &m_probeBarrieredDst,
                    &m_probeBareSrc, &m_probeBareDst})
    {
      delete *b;
      *b = nullptr;
    }
    m_scene = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
HazardProbeNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new HazardProbeRenderer{*this};
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  bool validationLayerPresent = false;
  bool rendered = false;
  std::string log;
};

Outcome run_probe(bool issueProbes)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    const int probe = p.addNode(std::make_unique<HazardProbeNode>(issueProbes));
    const int flat
        = p.addNode(std::make_unique<score::gfx::ScenePreprocessorNode>());
    const int raster = p.addRaster(
        corpus("syn-instance-index-color.vs"),
        corpus("syn-instance-index-color.fs"));
    if(probe < 0 || flat < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* sceneOut = p.nodeSceneOut(probe, 0);
    auto* flatIn = p.nodeSceneIn(flat, 0);
    auto* flatOut = p.nodeGeometryOut(flat, 0);
    if(!sceneOut || !flatIn || !flatOut)
    {
      out.error = "scene ports missing on the chain";
      return;
    }
    p.wire(sceneOut, flatIn);
    p.wire(flatOut, p.geometryIn(raster, 0));
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(score::gfx::Vulkan))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

#if QT_HAS_VULKAN
    if(auto* inst = score::gfx::staticVulkanInstance(/*create=*/false))
      out.validationLayerPresent = inst->supportedLayers().contains(
          QByteArrayLiteral("VK_LAYER_KHRONOS_validation"));
#endif

#if GFX_SYNC_HAVE_STDERR_CAPTURE
    StderrCapture cap;
#endif
    // Four frames: the first allocates the inst.* buffers (growBuf's
    // full-extent zero-clear) and issues the per-instance copies into them;
    // the rest keep the steady state honest.
    p.render(4);
    const auto img = p.readback(sink);
    out.rendered = img.width == kSize && img.height == kSize
                   && !img.bytes.isEmpty();
#if GFX_SYNC_HAVE_STDERR_CAPTURE
    out.log = cap.finish();
#endif
  });
  return out;
}

} // namespace

TEST_CASE(
    "raw buffer copies are ordered against the resource-update batch's own "
    "transfer writes",
    "[gfx][vulkan][sync][scene][instancer]")
{
  (void)g_env_init;

#if !GFX_SYNC_HAVE_STDERR_CAPTURE
  SKIP("no fd-2 capture on this platform");
#else
  // Vulkan-only by construction: synchronization validation is a Vulkan layer,
  // and the barrier this pins exists only on that backend. Honour an explicit
  // SCORE_TEST_API pointing elsewhere instead of spinning up a second QRhi.
  {
    const auto backends = platform_backends();
    if(std::find(backends.begin(), backends.end(), score::gfx::Vulkan)
       == backends.end())
      SKIP("Vulkan-specific; SCORE_TEST_API selects another backend");
  }

  // --- Run 1: the probes, in a pipeline of their own. -----------------------
  const auto probes = run_probe(/*issueProbes=*/true);
  if(probes.skipped)
    SKIP("Vulkan unavailable: " + probes.skip_reason);

  INFO("backend=" << probes.backend << " error=" << probes.error);
  REQUIRE(probes.error.empty());
  REQUIRE(probes.rendered);

  if(!probes.validationLayerPresent)
    SKIP("VK_LAYER_KHRONOS_validation not available on this machine");

  // "bare_dst" is not a substring of "barriered_dst", so the two counts are
  // independent.
  const int bare = hazardLines(probes.log, "bare_dst");
  const int barriered = hazardLines(probes.log, "barriered_dst");
  INFO(
      "probe run: total SYNC-HAZARD lines=" << hazardLines(probes.log, "")
                                            << " bare_dst=" << bare
                                            << " barriered_dst=" << barriered);

  // POSITIVE CONTROL, established BEFORE anything is concluded from silence.
  // An unsynchronised copy over a range the update batch just wrote is a
  // hazard by construction; if the layer does not say so it is not watching,
  // and every zero below would be meaningless.
  if(bare == 0)
    SKIP(
        "synchronization validation is not reporting the deliberate "
        "unsynchronised copy; the layer's sync checks are off on this run "
        "(total SYNC-HAZARD lines seen: "
        + std::to_string(hazardLines(probes.log, "")) + ")");

  // Same copy, same frame, same buffers -- only the barrier bracket differs.
  CHECK(barriered == 0);

  // --- Run 2: the engine path, with no probe barrier to shield it. ----------
  const auto engine = run_probe(/*issueProbes=*/false);
  INFO("engine run: error=" << engine.error);
  REQUIRE(engine.error.empty());
  REQUIRE(engine.rendered);

  const int preprocessor = hazardLines(engine.log, "ScenePreprocessor::inst.");
  const int transforms
      = hazardLines(engine.log, "BufferCopyBarrierTest::transforms");
  INFO(
      "engine run: total SYNC-HAZARD lines="
      << hazardLines(engine.log, "")
      << " ScenePreprocessor::inst.*=" << preprocessor
      << " source transforms=" << transforms);

  // The WRITE_AFTER_WRITE this whole file is about: growBuf's full-extent
  // zero-clear versus issuePendingGpuCopies' per-instance writes.
  CHECK(preprocessor == 0);
  // And its mirror image on the source the producer had just uploaded.
  CHECK(transforms == 0);
#endif
}
