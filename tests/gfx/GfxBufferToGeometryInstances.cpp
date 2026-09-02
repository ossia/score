// =============================================================================
// P1-10 -- THE "INSTANCES" SPINBOX ON BUFFERS-TO-GEOMETRY DRIVES THE DRAWN
// COUNT: the LIVE-RENDER half of tests/threedim/BufferToGeometryTest.cpp.
//
// (The CPU test pins the descriptor: `mesh.instances` is the Instances
// control passed through verbatim. This file adds what it cannot: the REAL
// Threedim::BuffersToGeometry2 ticked per frame, its halp geometry converted
// through the REAL avnd conversion layer, published as an
// ossia::geometry_spec to a REAL raw-raster consumer whose CustomMesh issues
// cb.draw(g.vertices, g.instances) (CustomMesh.cpp:724) on a real backend,
// read back as pixels -- with the reload count taken from the engine's own
// [BUFTRACE] "CustomMesh::reload" trace.)
//
// Intended registration (tests/gfx/CMakeLists.txt), mirroring the
// test_gfx_instancer_shrink block -- BufferToGeometry2.cpp is
// hidden-visibility inside score_plugin_threedim, so it is compiled into the
// test target (exactly as tests/threedim/CMakeLists.txt:327-333 does for the
// CPU test):
//
//   if(TARGET score_plugin_threedim)
//     score_plugin_hidden_sources(_b2g_instances_hidden
//         "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/BufferToGeometry2.cpp")
//     score_add_test(test_gfx_buffer_to_geometry_instances
//       SOURCES GfxBufferToGeometryInstances.cpp ${_b2g_instances_hidden}
//       GUI
//       PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
//       LIBS test_gfx_engine_glue)
//     target_compile_definitions(test_gfx_buffer_to_geometry_instances PRIVATE
//       GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
//     target_include_directories(test_gfx_buffer_to_geometry_instances SYSTEM PRIVATE
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
//       $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
//       $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
//   endif()
//
// WHICH SIBLING. BuffersToGeometry2 (c_name buffers_to_geometry_v2) is
// driven: v1 (BufferToGeometry.cpp) carries halp_flag(deprecated) and every
// new document instantiates v2. On the axis under test the two are
// line-for-line identical -- same fingerprint block with the same omission
// (v1 BufferToGeometry.cpp:118-129 / v2 BufferToGeometry2.cpp:84-93), same
// verbatim write `mesh.instances = inputs.instances.value` (v1 :239 /
// v2 :204), same `out.dirty_mesh = meshChanged` publish (v1 :320 / v2 :290)
// -- so every finding here reads onto v1 unchanged.
//
// ENGINE SURFACE DRIVEN (all verified in source, this worktree):
//  * BuffersToGeometry2::operator()() -- the real change fingerprint
//    (BufferToGeometry2.cpp:84-110: config compare 84-93, per-attribute
//    compare 96-110 which compares ALL 8 slots whether enabled or not), the
//    no-change early return (:131-148), the descriptor rebuild with
//    `mesh.instances = inputs.instances.value` (:204) and
//    `out.dirty_mesh = meshChanged` (:290).
//  * The avnd geometry conversion the shipped Crousti wrapper runs per frame:
//    oscr::load_geometry / oscr::update_geometry
//    (3rdparty/avendish/include/avnd/binding/ossia/geometry.hpp:356 / :756;
//    :765-767 is the `geom.instances != ctrl.instances -> need_reload`
//    check). The harness renderer mirrors geometry_outputs_storage::upload
//    (score-plugin-avnd/Crousti/GpuUtils.hpp:1636-1692) verbatim: dirty_mesh
//    -> reload_mesh builds a NEW ossia::mesh_list (GpuUtils.hpp:1614-1633),
//    else update_geometry, need_reload -> reload_mesh.
//  * RenderList::acquireMesh (RenderList.cpp:684). SPEC NOTE, deviating from
//    the P1-10 sketch with reason: the sketch says the control "bumps
//    mesh_list::dirty_index" (PATH 1a, RenderList.cpp:715-733). For THIS
//    node the production reload channel is dirty_mesh -> a FRESH mesh_list
//    shared_ptr every rebuild (GpuUtils.hpp:1616), which acquireMesh picks up
//    as a cache re-key, PATH 2 (RenderList.cpp:762-800); dirty_index of the
//    fresh list is not what triggers it. Both paths funnel into
//    CustomMesh::reload -- the observable counted here -- so the "exactly one
//    reload per change" contract is asserted where the code really lives.
//  * CustomMesh::reload / draw -- vertex layout from the published bindings
//    and per_instance classification (CustomMesh.cpp:527-560), and the
//    non-indexed draw `cb.draw(g.vertices, g.instances)` (CustomMesh.cpp:724).
//
// RELOAD COUNTING (what gates BUFTRACE, verified): the "CustomMesh::reload"
// line (CustomMesh.cpp:528-532) is a plain qDebug behind the BUFTRACE()
// macro (CustomMesh.hpp:20), which is RUNTIME-gated only:
// buftrace_enabled() (CustomMesh.cpp:15-22) returns true unless the
// SCORE_BUFTRACE env var is set to a string starting with '0'. Nothing
// compiles it out in release (no QT_NO_DEBUG_OUTPUT anywhere in the build),
// so the line is emitted unconditionally at compile time and by default at
// runtime. The test still forces SCORE_BUFTRACE=1 and re-enables the default
// Qt logging category defensively, then captures fd 2 with the dup2 pattern
// from tests/gfx/GfxEdgeConsumeLatch.cpp and counts occurrences. Unix-only,
// like the precedent; on other platforms the reload-count checks are
// skipped while every pixel assertion still runs.
//
// SCENARIO. One session, one create(), no graph rebuild. The node gets two
// test-owned QRhiBuffers: buffer 0 = a 6-vertex float4 quad 2 px wide and
// full-height; buffer 1 = 8 float4 per-instance translations, instance i at
// NDC x = -1 + 0.25 i (8 px apart on the 64-px frame) with w = i/255 as a
// buffer-identity channel. Consumer: corpus/syn-instance-index-color.{vs,fs}
// (same closed forms as GfxInstancerShrink / GfxInstanceCountLive): each
// drawn strip i covers pixel columns [8i, 8i+2) with R=255, G=i (identity
// read from the translation buffer), B=i (gl_InstanceIndex of the draw).
//
//   Phase A: Instances = 4 (initial build)  -> 4 disjoint strips.
//   Phase B: Instances = 8 PLUS one fingerprint nudge -> 8 strips, and
//            exactly ONE CustomMesh::reload in the phase window.
//   Phase C: Instances 8 -> 2, NOTHING else  -> [ENGINE GAP, pinned]: the
//            frame FREEZES at 8 strips, ZERO reloads, byte-identical image.
//   Phase D: the same nudge again (Instances still 2) -> the stale value
//            flushes: 2 strips, exactly one reload.
//
// THE GAP, stated plainly (found writing this test, pinned GREEN as current
// behavior): `instances` is the ONE geometry control missing from the
// change fingerprint. BufferToGeometry2.cpp:84-93 compares vertices /
// topology / cull / front-face / index state, :96-110 the attribute slots --
// `inputs.instances.value` appears in neither and no m_prevInstances member
// exists (BufferToGeometry2.hpp:100-110). An Instances-only edit therefore
// takes the early return (:131-148): `mesh.instances` keeps its old value,
// dirty_mesh stays false, and the downstream converter's own
// `geom.instances != ctrl.instances` check (avnd geometry.hpp:765-767)
// never sees the new number because the node never wrote it. The drawn
// count freezes until ANY fingerprinted control changes (phase D). The
// product fix is one line each in v1/v2: add an m_prevInstances compare to
// the :84-93 block. When that lands, phase C's three gap CHECKs go red --
// flip them to the fixed expectation (2 strips, one reload) and delete the
// nudge from phase B.
//
// The "nudge" is deliberately the most inert fingerprint hit that exists:
// bumping `Attr7 offset` on a DISABLED attribute slot (buffer = -1). The
// :96-110 compare fires on all 8 slots regardless of enabled, while the
// rebuild skips disabled slots entirely -- so the republished descriptor is
// byte-identical except for `instances`. Phase B is therefore still an
// honest "the Instances value drives the drawn count" probe; the nudge is
// only the doorbell the current fingerprint requires.
//
// NEGATIVE CONTROL (product-side, one line, for the orchestrator; the
// spec's "stop bumping dirty_index" translated to this node's real dirty
// channel): in src/plugins/score-plugin-threedim/Threedim/
// BufferToGeometry2.cpp:290 change
//   `out.dirty_mesh = meshChanged;`  to  `out.dirty_mesh = false;`
// -- no rebuild is ever announced, the converter never reloads, the drawn
// count freezes at the initial 4 forever: phase B's litRuns == 8 and
// reloadsB == 1 go red, phase D's litRuns == 2 and reloadsD == 1 go red.
// (An acquireMesh-level control -- neutering the PATH 2 re-key at
// RenderList.cpp:762-800 -- fires the same assertions from the consumer
// side, but touches a path shared by every geometry test; the one above is
// scoped to the node this test is about.)
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_buffer_to_geometry_instances
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_buffer_to_geometry_instances
// The verdict is pixels: unavailable backends SKIP, never fall back to Null.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <score/tools/Debug.hpp> // SCORE_ASSERT, used by avnd geometry.hpp

#include <Threedim/BufferToGeometry2.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <avnd/binding/ossia/geometry.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QLoggingCategory>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#if defined(__unix__)
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
QString corpus(const char* f)
{
  return QStringLiteral(GFX_TEST_CORPUS_DIR "/") + QString::fromUtf8(f);
}

constexpr int kSize = 64;         // frame is 64 x 64
constexpr int kStripPx = 8;       // instance i owns columns [8i, 8i+2)
constexpr float kQuadW = 0.0625f; // prototype quad width: 2 px in NDC
constexpr int kMaxInstances = 8;  // translation buffer capacity
constexpr int kCountA = 4;
constexpr int kCountB = 8;
constexpr int kCountCD = 2;
constexpr int kTol = 2; // LSB rounding across backends

// --- stderr capture (dup2 pattern from GfxEdgeConsumeLatch.cpp) -------------

#if defined(__unix__)
/// Captures everything written to fd 2 between construction and finish().
/// BUFTRACE goes through qDebug -> the default handler's stderr write, so it
/// lands here; keep windows to a few 64x64 frames so the 64 KB pipe never
/// fills (a full pipe would block the writer in-process).
struct StderrCapture
{
  int saved{-1};
  int fds[2]{-1, -1};

  StderrCapture()
  {
    ::fflush(stderr);
    saved = ::dup(2);
    ::pipe(fds);
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

int countReloads(const std::string& log)
{
  static const std::string needle = "CustomMesh::reload";
  int n = 0;
  for(std::size_t pos = 0; (pos = log.find(needle, pos)) != std::string::npos;
      pos += needle.size())
    ++n;
  return n;
}
#endif

// --- The harness node --------------------------------------------------------
//
// A data-only score::gfx producer (the pattern GfxInstancerShrink.cpp landed):
// its renderer owns the two QRhiBuffers, ticks the REAL
// Threedim::BuffersToGeometry2 every frame the way Crousti's wrapper does,
// converts the halp descriptor through the REAL oscr::load_geometry /
// update_geometry (mirroring geometry_outputs_storage::upload,
// GpuUtils.hpp:1636-1692), and publishes the ossia::geometry_spec to its
// output edge via NodeRenderer::process(port, spec, edge.source) -- the same
// publish every geometry producer uses.

struct B2GInstancesNode final : score::gfx::ProcessNode
{
  // The REAL engine object under test, ticked by the renderer.
  mutable Threedim::BuffersToGeometry2 b2g;

  // Test-driven phase; flip between render() calls.
  std::atomic<int> requestedPhase{1};
  std::atomic<int> appliedPhase{0};

  // CPU-side observations, read after render() on the same thread.
  mutable int cpuInstancesAfterTick{-1};

  B2GInstancesNode()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  ~B2GInstancesNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct B2GInstancesRenderer final : score::gfx::NodeRenderer
{
  B2GInstancesNode& self;
  ossia::geometry_spec m_spec;

  QRhiBuffer* m_posBuf{};
  QRhiBuffer* m_transBuf{};
  int m_nudge = 0;

  explicit B2GInstancesRenderer(const B2GInstancesNode& n)
      : NodeRenderer{n}
      , self{const_cast<B2GInstancesNode&>(n)}
  {
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    auto* rhi = r.state.rhi;

    // Buffer 0: the prototype quad, 6 x float4, 2 px wide, full height.
    m_posBuf = rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::UsageFlags(
            QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer),
        6 * 16);
    m_posBuf->setName("B2GInstancesTest::positions");
    m_posBuf->create();
    {
      const float pos[6 * 4] = {
          0.f,    -1.f, 0.f, 1.f, //
          kQuadW, -1.f, 0.f, 1.f, //
          kQuadW, 1.f,  0.f, 1.f, //
          0.f,    -1.f, 0.f, 1.f, //
          kQuadW, 1.f,  0.f, 1.f, //
          0.f,    1.f,  0.f, 1.f, //
      };
      res.uploadStaticBuffer(m_posBuf, 0, sizeof(pos), pos);
    }

    // Buffer 1: 8 per-instance float4 translations, 8 px apart, w = i/255
    // (the buffer-identity channel the fragment writes into G).
    m_transBuf = rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::UsageFlags(
            QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer),
        kMaxInstances * 16);
    m_transBuf->setName("B2GInstancesTest::translations");
    m_transBuf->create();
    {
      float tr[kMaxInstances * 4];
      for(int i = 0; i < kMaxInstances; ++i)
      {
        tr[4 * i + 0] = -1.f + 0.25f * float(i);
        tr[4 * i + 1] = 0.f;
        tr[4 * i + 2] = 0.f;
        tr[4 * i + 3] = float(i) / 255.f;
      }
      res.uploadStaticBuffer(m_transBuf, 0, sizeof(tr), tr);
    }

    // Static node configuration -- the exact controls the inspector edits.
    auto& in = self.b2g.inputs;
    in.buffer_0.buffer.handle = m_posBuf;
    in.buffer_0.buffer.byte_size = 6 * 16;
    in.buffer_0.buffer.byte_offset = 0;
    in.buffer_0.buffer.changed = false;
    in.buffer_1.buffer.handle = m_transBuf;
    in.buffer_1.buffer.byte_size = kMaxInstances * 16;
    in.buffer_1.buffer.byte_offset = 0;
    in.buffer_1.buffer.changed = false;

    // Attr 0: per-vertex position, float4, tightly packed.
    in.attribute_buffer_0.value = 0;
    in.attribute_offset_0.value = 0;
    in.attribute_stride_0.value = 16;
    in.format_0.value = Threedim::AttributeFormat::Float4;
    in.semantic_0.value = "position";
    in.instanced_0.value = false;

    // Attr 1: per-instance translation, float4 -- the binding the consumer's
    // "SEMANTIC": "translation" vertex input resolves to.
    in.attribute_buffer_1.value = 1;
    in.attribute_offset_1.value = 0;
    in.attribute_stride_1.value = 16;
    in.format_1.value = Threedim::AttributeFormat::Float4;
    in.semantic_1.value = "translation";
    in.instanced_1.value = true;

    in.vertices.value = 6;
    in.topology.value = Threedim::PrimitiveTopology::Triangles;
    in.cull_mode.value = Threedim::CullMode::None;
    in.front_face.value = Threedim::FrontFace::CounterClockwise;
    // index_buffer stays -1 (non-indexed draw).

    m_initialized = true;
  }

  // The most inert fingerprint hit that exists: Attr7 is DISABLED
  // (buffer = -1), but the compare loop (BufferToGeometry2.cpp:96-110) fires
  // on all 8 slots regardless of enabled, while the rebuild skips disabled
  // slots -- the republished descriptor differs ONLY in `instances`.
  void bumpNudge() { self.b2g.inputs.attribute_offset_7.value = ++m_nudge; }

  void configurePhase(int phase)
  {
    auto& in = self.b2g.inputs;
    switch(phase)
    {
      case 1: // initial build
        in.instances.value = kCountA;
        break;
      case 2: // grow, with the fingerprint doorbell
        in.instances.value = kCountB;
        bumpNudge();
        break;
      case 3: // shrink, Instances ONLY -> the pinned gap: nothing may move
        in.instances.value = kCountCD;
        break;
      case 4: // same value, doorbell only -> the stale count flushes
        bumpNudge();
        break;
    }
  }

  // Mirrors oscr::geometry_outputs_storage::reload_mesh
  // (GpuUtils.hpp:1614-1633): every rebuild publishes a FRESH mesh_list --
  // the identity change acquireMesh's PATH 2 re-keys on.
  void reloadSpec()
  {
    m_spec.meshes = std::make_shared<ossia::mesh_list>();
    m_spec.meshes->meshes.resize(1);
    oscr::load_geometry(self.b2g.outputs.geometry.mesh, m_spec.meshes->meshes[0]);
  }

  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&,
      score::gfx::Edge*) override
  {
    const int want = self.requestedPhase.load();
    if(want != self.appliedPhase.load())
    {
      configurePhase(want);
      self.appliedPhase.store(want);
    }

    // Per-frame tick, exactly as the Crousti CPU-node wrapper drives it.
    self.b2g();
    self.cpuInstancesAfterTick = self.b2g.outputs.geometry.mesh.instances;

    // The conversion the wrapper runs per frame
    // (geometry_outputs_storage::upload, GpuUtils.hpp:1636-1692).
    auto& ctrl = self.b2g.outputs.geometry;
    if(ctrl.dirty_mesh)
    {
      reloadSpec();
    }
    else if(m_spec.meshes && !m_spec.meshes->meshes.empty())
    {
      auto [need_reload, need_upload]
          = oscr::update_geometry(ctrl.mesh, m_spec.meshes->meshes[0]);
      if(need_reload)
        reloadSpec();
    }
    ctrl.dirty_mesh = false;
  }

  // Publish to the downstream sink exactly as every geometry producer does;
  // consumers dedupe on geometry_spec identity.
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(!m_spec.meshes)
      return;
    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;
    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;
    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;
    const int port_idx = int(it - sink->node->input.begin());
    rn_it->second->process(port_idx, m_spec, edge.source);
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }

  void release(score::gfx::RenderList&) override
  {
    // CustomMesh borrows the handles unowned (update_vbo(gpu),
    // CustomMesh.cpp:213-255), so the harness frees them.
    delete m_posBuf;
    m_posBuf = nullptr;
    delete m_transBuf;
    m_transBuf = nullptr;
    m_spec = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
B2GInstancesNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new B2GInstancesRenderer{*this};
}

// --- Pixel analysis ----------------------------------------------------------

struct ColumnStats
{
  int litColumns{};
  int litRuns{};
  int maxLitColumn{-1};
};

ColumnStats analyze(const ReadbackImage& img)
{
  ColumnStats s;
  bool prevLit = false;
  for(int x = 0; x < img.width; ++x)
  {
    bool lit = false;
    for(int y = 0; y < img.height && !lit; ++y)
    {
      const auto p = img.at(x, y);
      lit = int(p[0]) >= 200; // R == 1.0 is the drawn-coverage marker
    }
    if(lit)
    {
      ++s.litColumns;
      s.maxLitColumn = x;
      if(!prevLit)
        ++s.litRuns;
    }
    prevLit = lit;
  }
  return s;
}

struct Phase
{
  ColumnStats px{};
  bool valid = false;
  int cpuInstances = -1; // node.outputs.geometry.mesh.instances after tick
  int reloads = -1;      // "CustomMesh::reload" lines in the phase window
  // Per-strip identity samples at (kStripPx*i, kSize/2):
  // [i][0]=R [i][1]=G(buffer id) [i][2]=B(draw id)
  std::array<std::array<int, 3>, kMaxInstances> id{};
};

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  int appliedPhase = 0;

  Phase a, b, c, d;
  bool freezeIdentical = false; // phase C bytes == phase B bytes
  bool flushDiffers = false;    // phase D bytes != phase C bytes
};

void samplePhase(Phase& ph, const ReadbackImage& img)
{
  ph.valid = img.width == kSize && img.height == kSize && !img.bytes.isEmpty();
  if(!ph.valid)
    return;
  ph.px = analyze(img);
  for(int i = 0; i < kMaxInstances; ++i)
  {
    const auto p = img.at(i * kStripPx, kSize / 2);
    ph.id[i] = {int(p[0]), int(p[1]), int(p[2])};
  }
}

Outcome run_it(score::gfx::GraphicsApi api)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    auto harness_uptr = std::make_unique<B2GInstancesNode>();
    auto* harness = harness_uptr.get();

    const int hn = p.addNode(std::move(harness_uptr));
    const int raster = p.addRaster(
        corpus("syn-instance-index-color.vs"),
        corpus("syn-instance-index-color.fs"));
    if(hn < 0 || raster < 0)
    {
      out.error = "chain build failed: " + p.error();
      return;
    }

    auto* geoOut = p.nodeGeometryOut(hn, 0);
    auto* geoIn = p.geometryIn(raster, 0);
    if(!geoOut || !geoIn)
    {
      out.error = "geometry ports missing on the chain";
      return;
    }
    p.wire(geoOut, geoIn);
    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(api))
    {
      out.backend = p.backend();
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    // Runs one phase: flip the request, render a short settle window inside
    // a stderr capture, read back, count reloads.
    auto runPhase = [&](Phase& ph, int phase) {
      ph.reloads = -1;
#if defined(__unix__)
      StderrCapture cap;
#endif
      harness->requestedPhase.store(phase);
      p.render(4);
#if defined(__unix__)
      ph.reloads = countReloads(cap.finish());
#endif
      ph.cpuInstances = harness->cpuInstancesAfterTick;
      return p.readback(sink);
    };

    // Phase A: Instances = 4, initial build.
    const auto imgA = runPhase(out.a, 1);
    samplePhase(out.a, imgA);

    // Phase B: Instances = 8 + the fingerprint nudge.
    const auto imgB = runPhase(out.b, 2);
    samplePhase(out.b, imgB);

    // Phase C: Instances -> 2, NOTHING else. Gap: must currently freeze.
    const auto imgC = runPhase(out.c, 3);
    samplePhase(out.c, imgC);
    out.freezeIdentical
        = out.b.valid && out.c.valid && imgB.bytes == imgC.bytes;

    // Phase D: nudge only -> the stale value flushes to 2.
    const auto imgD = runPhase(out.d, 4);
    samplePhase(out.d, imgD);
    out.flushDiffers = out.c.valid && out.d.valid && imgC.bytes != imgD.bytes;

    out.appliedPhase = harness->appliedPhase.load();
  });
  return out;
}

void checkStrips(const Phase& ph, int count, const char* name)
{
  INFO(
      "phase " << name << ": litCols=" << ph.px.litColumns
               << " runs=" << ph.px.litRuns << " maxCol=" << ph.px.maxLitColumn
               << " cpuInstances=" << ph.cpuInstances
               << " reloads=" << ph.reloads);
  REQUIRE(ph.valid);
  // The drawn-instance counter: count disjoint 2-px strips, 8 px apart.
  CHECK(ph.px.litRuns == count);
  CHECK(ph.px.litColumns >= count);     // every strip >= 1 column
  CHECK(ph.px.litColumns <= 4 * count); // and <= 4 columns wide
  // Strip count-1 starts at column 8*(count-1); nothing right of it + slack.
  CHECK(ph.px.maxLitColumn <= kStripPx * (count - 1) + 3);
  CHECK(ph.px.maxLitColumn >= kStripPx * (count - 1));
  // Per-strip identities: G = the slice of the translation buffer that fed
  // the strip, B = the draw call's gl_InstanceIndex. Both must equal i.
  for(int i = 0; i < count; ++i)
  {
    INFO(
        "strip " << i << " RGB=(" << ph.id[i][0] << "," << ph.id[i][1] << ","
                 << ph.id[i][2] << ")");
    CHECK(ph.id[i][0] > 255 - 2 * kTol);          // drawn marker
    CHECK(std::abs(ph.id[i][1] - i) <= kTol);     // buffer identity
    CHECK(std::abs(ph.id[i][2] - i) <= kTol);     // draw identity
  }
}

} // namespace

TEST_CASE(
    "the Instances control on Buffers-to-geometry drives the drawn count "
    "live, one CustomMesh::reload per change (Instances-only edits are a "
    "pinned no-op gap)",
    "[gfx][threedim][buffertogeometry][instancing][p1-10]")
{
  // The reload observable: force the runtime gate ON (it is on by default --
  // CustomMesh.cpp:15-22 -- but a CI environment may export SCORE_BUFTRACE=0)
  // and make sure the default Qt logging category actually emits qDebug.
  qputenv("SCORE_BUFTRACE", "1");
  QLoggingCategory::setFilterRules(QStringLiteral("default.debug=true"));

  const auto api = GENERATE(from_range(platform_backends()));

  const auto r = run_it(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.appliedPhase == 4); // the harness renderer ran all phases

  // ---- Phase A: initial build at Instances = 4. ----
  checkStrips(r.a, kCountA, "A(4)");
  CHECK(r.a.cpuInstances == kCountA);

  // ---- Phase B: 4 -> 8, live, same session. ----
  checkStrips(r.b, kCountB, "B(8)");
  CHECK(r.b.cpuInstances == kCountB);

  // ---- Phase C: 8 -> 2, Instances ONLY. ENGINE GAP, pinned as current
  // behavior: the fingerprint (BufferToGeometry2.cpp:84-110) omits
  // `instances`, so the early return (:131-148) ships the STALE descriptor --
  // the drawn count freezes at 8 and nothing reloads. When the product adds
  // an m_prevInstances compare, these three CHECKs (and freezeIdentical) go
  // red: flip them to count 2 / one reload, and drop bumpNudge() from
  // configurePhase(2). ----
  checkStrips(r.c, kCountB, "C(2 requested, frozen at 8 -- the gap)");
  CHECK(r.c.cpuInstances == kCountB); // mesh.instances never rewritten
  CHECK(r.freezeIdentical);           // byte-identical frame: a true freeze

  // ---- Phase D: any fingerprint hit flushes the stale value -> 2. ----
  checkStrips(r.d, kCountCD, "D(2)");
  CHECK(r.d.cpuInstances == kCountCD);
  CHECK(r.flushDiffers);

#if defined(__unix__)
  // ---- The reload contract, from the engine's own BUFTRACE channel:
  // exactly one CustomMesh::reload per applied change, zero without. ----
  INFO(
      "reloads A=" << r.a.reloads << " B=" << r.b.reloads
                   << " C=" << r.c.reloads << " D=" << r.d.reloads);
  CHECK(r.a.reloads >= 1); // initial acquireMesh PATH 3 (fresh mesh)
  CHECK(r.b.reloads == 1); // the 4 -> 8 change: one reload, not per-frame
  CHECK(r.c.reloads == 0); // the gap: no dirty channel fired at all
  CHECK(r.d.reloads == 1); // the flush: again exactly one
#else
  WARN("reload counting skipped: stderr capture via dup2 is unix-only "
       "(GfxEdgeConsumeLatch.cpp precedent); pixel assertions above still "
       "ran in full");
#endif
}
