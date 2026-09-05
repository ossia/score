// =============================================================================
// P1-11 -- A POINT CLOUD RENDERS AS N POINTS: the LIVE-RENDER half, with the
// byte_offset arithmetic guarded at render level.
//
// (The CPU descriptor arithmetic of Threedim::PCLToMesh2 -- "Pointcloud to
// mesh", the node behind 19 of the 28 real Model Displays that run in Points
// mode -- is already pinned by tests/threedim/PCLToGeometryTest.cpp, including
// the byte_offset case: with a non-zero byte_offset the vertex count must be
// (byte_size - byte_offset) / stride, NOT byte_size / stride. That over-read
// defect was FIXED in 2b6234a6c3, so this file is a REGRESSION GUARD, not an
// expected-red pin. What no test has yet is the render half: the REAL
// PCLToMesh2 descriptor, converted through the REAL avnd geometry bridge,
// consumed by a REAL raw-raster draw in Points topology on a real backend,
// with the drawn point count counted from the frame.)
//
// Intended registration (tests/gfx/CMakeLists.txt), mirroring the
// test_gfx_instancer_shrink block -- PCLToGeometry.cpp is compiled into the
// plugin, so build it into the test target the same way
// tests/threedim/CMakeLists.txt:257-258 does for the CPU test:
//
//   if(TARGET score_plugin_threedim)
//     score_plugin_hidden_sources(_pcl_count_hidden
//         "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim/Threedim/PCLToGeometry.cpp")
//     score_add_test(test_gfx_pointcloud_count
//       SOURCES GfxPointCloudCount.cpp ${_pcl_count_hidden}
//       GUI
//       PLUGINS score_plugin_gfx score_plugin_scenario score_lib_process
//       LIBS test_gfx_engine_glue)
//     target_compile_definitions(test_gfx_pointcloud_count PRIVATE
//       GFX_TEST_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/corpus")
//     target_include_directories(test_gfx_pointcloud_count SYSTEM PRIVATE
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-threedim"
//       "${SCORE_ROOT_SOURCE_DIR}/src/plugins/score-plugin-gfx"
//       "${SCORE_ROOT_BINARY_DIR}/src/plugins/score-plugin-gfx"
//       $<TARGET_PROPERTY:score_plugin_threedim,INCLUDE_DIRECTORIES>
//       $<TARGET_PROPERTY:score_plugin_gfx,INCLUDE_DIRECTORIES>)
//   endif()
//
// WHAT LEVEL IS DRIVEN, AND WHY. Everything from the PCLToMesh2 tick down to
// the readback is the shipped engine:
//   * Threedim::PCLToMesh2::operator()() -- the real descriptor build,
//     including the FIXED count arithmetic (PCLToGeometry.cpp:120-122:
//     usable_bytes = byte_size - byte_offset; vertices = usable / stride) and
//     the pass-through of the input's byte_offset into the geometry input
//     (PCLToGeometry.cpp:114).
//   * oscr::load_geometry (avnd/binding/ossia/geometry.hpp:356) -- the real
//     halp::dynamic_gpu_geometry -> ossia::geometry bridge, the same function
//     Crousti's geometry_outputs_storage::reload_mesh calls for this exact
//     node in a real score (GpuUtils.hpp:1614-1633). It carries the GPU
//     handle, the Points topology, the position attribute, and CRUCIALLY the
//     input byte_offset and the vertex count into the ossia mesh.
//   * The publish is the production call, verbatim: NodeRenderer::process(
//     port, geometry_spec, edge.source) -- GpuUtils.hpp:1691 -- which lands
//     in NodeRenderer.cpp:536 and raises geometryChanged on the raw-raster
//     consumer.
//   * The draw is the real CustomMesh path: topology cast at
//     CustomMesh.cpp:583 (ossia points -> QRhiGraphicsPipeline::Points, enum
//     orders match), pipeline topology at :515, the vertex-buffer BIND OFFSET
//     taken from geom.input[i].byte_offset at CustomMesh.cpp:614, and the
//     drawn vertex count taken from geom.vertices at CustomMesh.cpp:724
//     (cb.draw(g.vertices, g.instances)).
//
// The ONE thing the test supplies instead of an upstream producer is the
// QRhiBuffer holding the float3 positions (the spec's shape is CSF ->
// Pointcloud to mesh; a CSF's buffer output cannot yet be routed into a
// data-only harness -- the documented P1-9 gap). This mirrors
// GfxInstancerShrink.cpp exactly, and is what lets the buffer carry SENTINEL
// data (below) that makes both regression directions deterministic.
//
// SCENARIO -- one buffer, three regions, every point on its own pixel center:
//
//   bytes [0,   64)  JUNK PREFIX the descriptor tells the renderer to SKIP:
//                    byte_offset = 64 (16 floats: 5 sentinel points + 1 pad).
//   bytes [64, 508)  THE CLOUD: 37 real XYZ points (stride 12).
//   bytes [508,568)  TAIL: 5 more sentinel points. The published byte_size is
//                    508, so these are past the END of what PCLToMesh2 is
//                    told about -- but INSIDE the real 568-byte QRhiBuffer.
//
//   PCLToMesh2 is fed {handle, byte_size = 508, byte_offset = 64}, XYZ.
//   Correct arithmetic: vertices = (508 - 64) / 12 = 37, fetch starts at
//   byte 64. The draw reads bytes [64, 508): exactly the 37 real points.
//
// All 47 points sit at distinct COLUMNS of the 64x64 frame (columns 8..54,
// one point per column, varying rows), each positioned exactly on a pixel
// center with gl_PointSize = 1.0 -- one point, one pixel, no overlap, and the
// column identifies the point regardless of the backend's Y orientation.
//
// THE ORACLE -- counted from the frame:
//   * exactly 37 lit pixels in total;
//   * each of the 37 real columns holds exactly 1 lit pixel, at the authored
//     row (or its Y-flip, uniformly for all points);
//   * the 5 prefix-sentinel columns and the 5 tail-sentinel columns hold NO
//     lit pixel.
// Both historical defect directions go red deterministically, on every
// backend, with no reliance on out-of-bounds reads returning garbage:
//   * count regression to byte_size / stride (the pre-2b6234a6c3 defect):
//     vertices = 508 / 12 = 42 -> the draw walks 42 * 12 bytes from offset 64
//     and renders the 5 TAIL sentinels -- valid, authored, on-screen points
//     inside the real buffer -- so the frame shows 42 lit pixels and the tail
//     columns light up. (In production the tail would be an over-read past
//     the buffer; here it is made visible instead of merely invalid.)
//   * byte_offset dropped anywhere down the chain (descriptor, bridge, or
//     the CustomMesh.cpp:614 bind offset): the fetch starts at byte 0 and
//     renders the 5 PREFIX sentinels; their columns light up and the real
//     column set breaks.
//
// GEOMETRY INFO CROSS-CHECK (the spec's second, independent measurement): the
// spec asks for an OSC readback of the Geometry Info process's vertex-count
// outlet; that is app-level (a device tree + OSC protocol + the Geometry Info
// process) and out of reach of this unit fixture, so it is deliberately NOT
// covered here. The same quantity is cross-checked CPU-side instead, at both
// ends of the bridge: PCLToMesh2's own descriptor (mesh.vertices == 37,
// input[0].byte_offset == 64) and the converted ossia mesh the renderer
// actually consumed (vertices == 37, input[0].byte_offset == 64, topology ==
// points, gpu handle identity). Pixels and descriptor are computed by
// different subsystems, so they are still two measurements.
//
// NEGATIVE CONTROLS (product-side, one line each, for the orchestrator):
//   * Reintroduce the fixed defect:
//     src/plugins/score-plugin-threedim/Threedim/PCLToGeometry.cpp:120-121 --
//     replace
//       const auto usable_bytes
//           = tex.byte_size > tex.byte_offset ? tex.byte_size - tex.byte_offset : 0;
//     with
//       const auto usable_bytes = tex.byte_size;
//     -> cpuVertices becomes 42 (CPU check red) AND the frame shows 42 lit
//     pixels with the 5 tail-sentinel columns lit (pixel checks red).
//   * The spec's suggested control -- halve the count:
//     src/plugins/score-plugin-threedim/Threedim/PCLToGeometry.cpp:122 --
//     append "/ 2" to the vertices expression -> 18 vertices; CPU check and
//     the lit-pixel count (18 != 37) go red together, proving the two
//     measurements are independent.
//   * Render-level offset drop:
//     src/plugins/score-plugin-gfx/Gfx/Graph/CustomMesh.cpp:614 -- replace
//     "in.byte_offset" with "0" -> the 5 prefix-sentinel columns light up.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_pointcloud_count
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_pointcloud_count
// The verdict is pixels: unavailable backends SKIP, never fall back to Null.
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Threedim/PCLToGeometry.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <avnd/binding/ossia/geometry.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <algorithm>
#include <array>
#include <atomic>
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

constexpr int kSize = 64; // frame is 64 x 64

// Point layout: 47 authored points, one per column, all on pixel centers.
constexpr int kPrefixPts = 5;  // sentinels inside the skipped byte_offset region
constexpr int kRealPts = 37;   // the cloud: what must be drawn
constexpr int kTailPts = 5;    // sentinels past the published byte_size
constexpr int kAllPts = kPrefixPts + kRealPts + kTailPts; // 47

constexpr int kStride = 12; // XYZ layout: 3 floats per point

// The junk prefix is 16 floats (5 sentinel points + 1 pad float) = 64 bytes.
constexpr int kPrefixFloats = 16;
constexpr int64_t kByteOffset = kPrefixFloats * sizeof(float); // 64
// What PCLToMesh2 is told: offset + the 37 real points. (508; NOT the real
// buffer size.) 508 / 12 = 42 is the pre-fix over-count; (508 - 64) / 12 = 37
// is the fixed count.
constexpr int64_t kByteSize = kByteOffset + kRealPts * kStride; // 508
// The real QRhiBuffer additionally holds the 5 tail sentinels, so a
// regression to the 42-count draw renders authored data, not garbage.
constexpr int kTotalFloats = kPrefixFloats + (kRealPts + kTailPts) * 3; // 142
constexpr int kBufBytes = kTotalFloats * sizeof(float); // 568

// Overall point index k (0..46) -> pixel. Distinct column per point; rows
// vary so a stuck row would be caught too.
constexpr int colOf(int k)
{
  return 8 + k; // columns 8..54, well inside the 64-wide frame
}
constexpr int rowOf(int k)
{
  return 6 + (k * 11) % 52; // rows 6..57
}

// Pixel center -> NDC. (2c+1)/64 - 1 is exact in binary floating point, so
// the point lands exactly on the pixel center and gl_PointSize = 1.0
// rasterizes exactly that one pixel on every backend.
constexpr float ndcOf(int pixel)
{
  return float(2 * pixel + 1) / float(kSize) - 1.f;
}

// --- The harness node --------------------------------------------------------
//
// A data-only score::gfx geometry producer (the pattern proven by
// GfxInstancerShrink.cpp): its renderer owns the position QRhiBuffer, ticks
// the REAL Threedim::PCLToMesh2 against it, converts the halp descriptor with
// the REAL oscr::load_geometry, and publishes the geometry_spec to its output
// edge with the exact production call Crousti's geometry_outputs_storage uses
// (GpuUtils.hpp:1691): NodeRenderer::process(port, spec, edge.source).

struct PclPointsNode final : score::gfx::ProcessNode
{
  // The REAL engine object under test, ticked by the renderer.
  mutable Threedim::PCLToMesh2 pcl;

  // Exposed for the CPU-side assertions (same thread as render() in this
  // synchronous offscreen fixture).
  std::atomic<bool> ticked{false};
  mutable QRhiBuffer* cloudBuf{};
  mutable ossia::geometry_spec spec; // what was actually published

  PclPointsNode()
  {
    output.push_back(
        new score::gfx::Port{this, {}, score::gfx::Types::Geometry, {}});
  }
  ~PclPointsNode() override = default;

  score::gfx::NodeRenderer* createRenderer(score::gfx::RenderList&) const
      noexcept override;
};

struct PclPointsRenderer final : score::gfx::NodeRenderer
{
  PclPointsNode& self;

  explicit PclPointsRenderer(const PclPointsNode& n)
      : NodeRenderer{n}
      , self{const_cast<PclPointsNode&>(n)}
  {
  }

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res) override
  {
    auto* rhi = r.state.rhi;

    self.cloudBuf = rhi->newBuffer(
        QRhiBuffer::Static,
        QRhiBuffer::UsageFlags(
            score::gfx::compatibleBufferUsage(
                *rhi, QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer)),
        kBufBytes);
    self.cloudBuf->setName("PclPointsTest::cloud");
    self.cloudBuf->create();

    // Author the three regions. z = 0 everywhere; w comes from the vertex
    // fetch expansion / the shader's vec4(position.xyz, 1.0).
    std::vector<float> data(kTotalFloats, 0.f);
    auto put = [&](int floatIdx, int k) {
      data[floatIdx + 0] = ndcOf(colOf(k));
      data[floatIdx + 1] = ndcOf(rowOf(k));
      data[floatIdx + 2] = 0.f;
    };
    for(int k = 0; k < kPrefixPts; ++k) // bytes [0, 60): prefix sentinels
      put(3 * k, k);
    data[kPrefixFloats - 1] = 777.777f; // bytes [60, 64): junk pad
    for(int i = 0; i < kRealPts; ++i)   // bytes [64, 508): the cloud
      put(kPrefixFloats + 3 * i, kPrefixPts + i);
    for(int t = 0; t < kTailPts; ++t)   // bytes [508, 568): tail sentinels
      put(kPrefixFloats + 3 * (kRealPts + t), kPrefixPts + kRealPts + t);

    res.uploadStaticBuffer(
        self.cloudBuf, 0, quint32(data.size() * sizeof(float)), data.data());

    m_initialized = true;
  }

  void update(
      score::gfx::RenderList&, QRhiResourceUpdateBatch&,
      score::gfx::Edge*) override
  {
    if(self.ticked.load())
      return;

    // Feed the REAL node the P1-11 shape: a GPU buffer whose usable region
    // starts kByteOffset bytes in. This mirrors what a CSF buffer producer
    // publishes into "Pointcloud to mesh" in a real score.
    auto& in = self.pcl.inputs.in.buffer;
    in.handle = self.cloudBuf;
    in.byte_size = kByteSize;
    in.byte_offset = kByteOffset;
    self.pcl.inputs.type.value = Threedim::PCLToMesh2::XYZ;

    // Tick the real engine object: the descriptor build under test.
    self.pcl();

    // Convert with the REAL bridge -- the same call reload_mesh
    // (Crousti/GpuUtils.hpp:1628) makes for this node's halp geometry output
    // in a running score.
    self.spec.meshes = std::make_shared<ossia::mesh_list>();
    self.spec.meshes->meshes.resize(1);
    oscr::load_geometry(self.pcl.outputs.geometry.mesh, self.spec.meshes->meshes[0]);
    self.spec.meshes->dirty_index++;

    self.ticked.store(true);
  }

  // Publish to the downstream raster exactly as Crousti's
  // geometry_outputs_storage::upload does (GpuUtils.hpp:1680-1691) -- every
  // frame; the consumer short-circuits on spec identity and buffer dirt.
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&,
      QRhiResourceUpdateBatch*&, score::gfx::Edge& edge) override
  {
    if(!self.spec.meshes)
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
    rn_it->second->process(port_idx, self.spec, edge.source);
    // No transform3d publish: PCLToMesh2's TRS controls are at their
    // defaults (identity), and the consumer's MODEL_MATRIX default is
    // identity too, so the production transform push would be a no-op.
  }

  void runRenderPass(
      score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }

  void release(score::gfx::RenderList&) override
  {
    delete self.cloudBuf;
    self.cloudBuf = nullptr;
    self.spec = {};
    m_initialized = false;
  }
};

score::gfx::NodeRenderer*
PclPointsNode::createRenderer(score::gfx::RenderList&) const noexcept
{
  return new PclPointsRenderer{*this};
}

// --- Pixel analysis ----------------------------------------------------------

struct PixelStats
{
  int totalLit = 0;
  std::array<int, kSize> colLit{};  // lit pixels per column
  std::array<int, kSize> colRow{};  // row of the first lit pixel per column
};

PixelStats analyze(const ReadbackImage& img)
{
  PixelStats s;
  s.colRow.fill(-1);
  for(int x = 0; x < img.width && x < kSize; ++x)
  {
    for(int y = 0; y < img.height; ++y)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) >= 200) // R == 1.0 is the drawn-point marker
      {
        ++s.totalLit;
        if(qEnvironmentVariableIsSet("GFX_DUMP"))
          std::fprintf(stderr, "[lit] x=%d y=%d\n", x, y);
        if(s.colLit[x]++ == 0)
          s.colRow[x] = y;
      }
    }
  }
  return s;
}

struct Outcome
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  bool ticked = false;

  // The node's own descriptor (the fixed arithmetic).
  int cpuVertices = -1;
  int64_t cpuInputOffset = -1;
  int cpuStride = -1;
  bool cpuTopologyPoints = false;

  // The converted ossia mesh the renderer consumed (the bridge).
  int ossiaVertices = -1;
  int64_t ossiaInputOffset = -1;
  bool ossiaTopologyPoints = false;
  bool ossiaHandleOk = false;
  int64_t ossiaByteSize = -1;

  bool imgValid = false;
  PixelStats px{};
};

Outcome run_pointcloud(score::gfx::GraphicsApi api)
{
  Outcome out;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    auto harness_uptr = std::make_unique<PclPointsNode>();
    auto* harness = harness_uptr.get();

    const int hn = p.addNode(std::move(harness_uptr));
    const int raster = p.addRaster(
        corpus("syn-pcl-point-count.vs"), corpus("syn-pcl-point-count.fs"));
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

    // Frame 1: tick + publish; frame 2: the raster rebinds the new mesh and
    // draws; two extra frames of margin, as in the instancer twin.
    p.render(4);
    out.ticked = harness->ticked.load();

    // CPU cross-check 1 -- the descriptor PCLToMesh2 built (the fixed
    // PCLToGeometry.cpp:120-122 arithmetic, plus the offset pass-through
    // at :114).
    {
      const auto& mesh = harness->pcl.outputs.geometry.mesh;
      out.cpuVertices = mesh.vertices;
      if(!mesh.input.empty())
        out.cpuInputOffset = mesh.input[0].byte_offset;
      if(!mesh.bindings.empty())
        out.cpuStride = mesh.bindings[0].stride;
      out.cpuTopologyPoints
          = mesh.topology == halp::primitive_topology::points;
    }

    // CPU cross-check 2 -- the converted ossia mesh actually published to
    // the renderer (the oscr::load_geometry bridge must not lose the count,
    // the offset, the topology or the handle).
    if(harness->spec.meshes && !harness->spec.meshes->meshes.empty())
    {
      const auto& m = harness->spec.meshes->meshes[0];
      out.ossiaVertices = m.vertices;
      if(!m.input.empty())
        out.ossiaInputOffset = m.input[0].byte_offset;
      out.ossiaTopologyPoints = m.topology == ossia::geometry::points;
      if(!m.buffers.empty())
      {
        if(const auto* gpu
           = ossia::get_if<ossia::geometry::gpu_buffer>(&m.buffers[0].data))
        {
          out.ossiaHandleOk = gpu->handle == harness->cloudBuf;
          out.ossiaByteSize = gpu->byte_size;
        }
      }
    }

    const auto img = p.readback(sink);
    out.imgValid
        = img.width == kSize && img.height == kSize && !img.bytes.isEmpty();
    if(out.imgValid)
      out.px = analyze(img);
  });
  return out;
}

} // namespace

TEST_CASE(
    "a point cloud renders as N points: 37 written, 37 drawn, byte_offset "
    "region and byte_size tail never drawn",
    "[gfx][threedim][pcl][raster][p1-11]")
{
  const auto api = GENERATE(from_range(platform_backends()));

  const auto r = run_pointcloud(api);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);

  INFO("backend=" << r.backend << " error=" << r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.ticked); // the harness renderer actually ran the real node

  // ---- Measurement 1: the descriptor, at both ends of the bridge. ----
  // PCLToMesh2's own arithmetic: (508 - 64) / 12 = 37, NEVER 508 / 12 = 42.
  CHECK(r.cpuVertices == kRealPts);
  CHECK(r.cpuInputOffset == kByteOffset);
  CHECK(r.cpuStride == kStride);
  CHECK(r.cpuTopologyPoints);
  // The avnd bridge must deliver the same numbers to the renderer.
  CHECK(r.ossiaVertices == kRealPts);
  CHECK(r.ossiaInputOffset == kByteOffset);
  CHECK(r.ossiaTopologyPoints);
  CHECK(r.ossiaHandleOk);
  CHECK(r.ossiaByteSize == kByteSize);

  // ---- Measurement 2: the drawn point count, from the frame. ----
  REQUIRE(r.imgValid);
  INFO("totalLit=" << r.px.totalLit << " (expected " << kRealPts << ")");

  // Exactly one pixel per real point, nothing else anywhere in the frame.
  CHECK(r.px.totalLit == kRealPts);

  // Each real point lit its own column exactly once...
  int realColsLitOnce = 0;
  for(int i = 0; i < kRealPts; ++i)
  {
    const int c = colOf(kPrefixPts + i);
    if(r.px.colLit[c] == 1)
      ++realColsLitOnce;
    else
    {
      INFO("real point " << i << " column " << c << " lit " << r.px.colLit[c]
                         << " pixels");
      CHECK(r.px.colLit[c] == 1);
    }
  }
  CHECK(realColsLitOnce == kRealPts);

  // ...at the authored row, in ONE consistent Y orientation for the whole
  // frame (backends differ in Y direction; the flip is global, never mixed).
  int rowsDirect = 0, rowsFlipped = 0;
  for(int i = 0; i < kRealPts; ++i)
  {
    const int k = kPrefixPts + i;
    const int got = r.px.colRow[colOf(k)];
    if(got == rowOf(k))
      ++rowsDirect;
    if(got == (kSize - 1) - rowOf(k))
      ++rowsFlipped;
  }
  INFO("rowsDirect=" << rowsDirect << " rowsFlipped=" << rowsFlipped);
  CHECK((rowsDirect == kRealPts || rowsFlipped == kRealPts));

  // The byte_offset region was skipped: no prefix sentinel drawn.
  for(int k = 0; k < kPrefixPts; ++k)
  {
    INFO("prefix sentinel column " << colOf(k));
    CHECK(r.px.colLit[colOf(k)] == 0);
  }

  // The count stopped at byte_size: no tail sentinel drawn. This is the
  // 2b6234a6c3 regression guard -- a return to the byte_size / stride count
  // draws exactly these 5 authored points.
  for(int t = 0; t < kTailPts; ++t)
  {
    const int k = kPrefixPts + kRealPts + t;
    INFO("tail sentinel column " << colOf(k));
    CHECK(r.px.colLit[colOf(k)] == 0);
  }
}
