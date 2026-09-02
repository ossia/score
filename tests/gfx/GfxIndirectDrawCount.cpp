// =============================================================================
// P1-8 -- AN INDIRECT DRAW TAKES ITS COUNT FROM THE BUFFER.
//
// Intended registration: score_add_gfx_test(indirect_draw_count GfxIndirectDrawCount.cpp)
//
// Closes gap G9: 15 real scores carry indirect_draw_cmds / indirect_draw_indexed,
// and the only existing coverage (ShaderSweepWired's binding-indirect-draw)
// asserts merely that the binding "does not disturb the draw" -- nothing checks
// that the drawn count actually CAME FROM the GPU-written command buffer.
//
// A CSF compute shader (corpus/syn-indirect-count.cs) declares
//   "INSTANCE_COUNT": "8"            (FIXED -- the CPU-side geometry_spec
//                                     always says 8 instances)
//   "INDIRECT": { "COUNT": 1 }       (the engine allocates a zero-filled
//                                     5-word indirect command SSBO)
// and, every frame, writes the single indirect command ON THE GPU with
//   instanceCount = clamp(count, 0, 8)
// from the 'count' long control. It feeds the same raw-raster consumer as the
// instancing tests (corpus/syn-instance-index-color.{vs,fs}): instance i is a
// full-height quad on pixel columns [4i, 4i+4) of the 64x64 frame, drawn as
// R=255, G=i (translation-buffer identity), B=i (gl_InstanceIndex), A=255.
// The PER_INSTANCE pass populates translations for ALL 8 instances every
// frame, so whichever count the indirect command requests reads well-defined
// per-instance data.
//
// The control moves from 3 to 6 within ONE render session (one GfxPipeline,
// one create(), setControl between render() calls). The frame must show
// exactly 3 strips, then exactly 6.
//
// WHY A PASS PROVES THE INDIRECT PATH WAS TAKEN (the G9 asymmetry): the
// CPU-visible instance count is pinned at 8 and never changes -- no control
// touches it, no geometry rebuild happens. Every non-indirect draw the engine
// could possibly issue is cb.draw(6, /*instances=*/ 8) and would paint 8
// strips in BOTH phases; the zero-initialized command buffer, if never
// written, would paint 0 strips in both. Only a draw whose count is read out
// of the GPU-written command can paint 3 then 6. No trace line is needed --
// the count itself is the witness.
//
// ENGINE SURFACE DRIVEN (all verified in source, this worktree):
//  * "INDIRECT": { "COUNT": ... } on a geometry resource parses into
//    geometry_input::indirect_request (libisf isf.cpp:1286-1311; struct at
//    isf.hpp:428-432) and emits, in the compute GLSL, the std430 SSBO
//    `DrawIndirectCommand geo_indirect[]` with members {vertexCount,
//    instanceCount, firstVertex, baseVertex, firstInstance}
//    (isf.cpp:6356-6376).
//  * RenderedCSFNode allocates the zero-initialized command buffer with
//    QRhiBuffer::IndirectBuffer usage on Qt >= 6.12 (RenderedCSFNode.cpp:
//    3964-3989, binding.uses_indirect_draw = true at :3966), binds it to the
//    compute SRB (:3485-3489), and stamps it onto the output geometry as
//    out_geo.indirect_count (:2305-2309).
//  * CustomMesh picks the handle up: first_mesh.indirect_count.handle sets
//    useIndirectDraw = true in init (CustomMesh.cpp:141-151) and update
//    (:434-446; indirectDrawIndexed = (index.buffer >= 0), false here -- our
//    geometry is non-indexed).
//  * The draw dispatches on it (CustomMesh.cpp:680-717):
//      - GPU path (Qt >= 6.12 && caps.drawIndirect, set from
//        QRhi::DrawIndirect at RenderList.cpp:1580-1583 and copied to
//        gpuIndirectSupported at RenderedRawRasterPipelineNode.cpp:2462):
//        cb.drawIndirect(buf, 0, 1, 20) -- the GPU reads instanceCount from
//        the buffer; the CPU-side g.instances is NOT consulted.
//      - CPU fallback (no caps.drawIndirect, QRhi::ReadBackNonUniformBuffer
//        available): RenderedRawRasterPipelineNode::runInitialPasses
//        synchronously reads the SAME GPU-written buffer back every frame
//        (RenderedRawRasterPipelineNode.cpp:2947-2977) and the draw loop
//        issues cb.draw(cmd.index_or_vertex_count, cmd.instance_count, ...)
//        (CustomMesh.cpp:701-717). Same contract, same witness -- so this
//        test asserts pixels on BOTH paths and reports which one was active.
//  * 4-word vs 5-word command: the non-indexed GPU read is a 4-word
//    QRhiDrawIndirectCommand at stride 20, so its firstInstance slot reads
//    the buffer's word 3 (baseVertex); the CPU fallback reads word 4
//    (RenderList.cpp:810-822 documents the divergence). The corpus shader
//    writes BOTH words as 0, making the two paths bit-identical here.
//
// SKIP policy:
//  * Backend not available -> fixture skip (p.skipped()).
//  * Neither QRhi::DrawIndirect (Qt >= 6.12 + hardware) nor
//    QRhi::ReadBackNonUniformBuffer -> SKIP: the engine itself degrades
//    gracefully there (warns and draws stale/no commands,
//    RenderedRawRasterPipelineNode.cpp:2979-2996), so no count contract
//    exists to assert.
//
// NEGATIVE CONTROL (product side, for the orchestrator; spec P1-8): force
// useIndirectDraw = false at BOTH pick-up sites --
//   src/plugins/score-plugin-gfx/Gfx/Graph/CustomMesh.cpp:144
//     `ret.useIndirectDraw = true;`            -> `... = false;`
//   src/plugins/score-plugin-gfx/Gfx/Graph/CustomMesh.cpp:438
//     `output_meshbuf.useIndirectDraw = true;` -> `... = false;`
// The draw then falls through to the plain instanced path
// cb.draw(g.vertices, g.instances) with the CPU-side instances == 8: both
// phases paint 8 identical strips, and this test's "strips [count, 8) are
// absent" checks go red in BOTH phases -- exactly the spec's "both dispatches
// draw the same".
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_indirect_draw_count
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_indirect_draw_count
// QT_QPA_PLATFORM=offscreen must NOT be used: the Null backend renders a
// stable, self-consistent and completely wrong picture; the verdict here is
// pixels, so unavailable backends SKIP instead.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <ossia/network/value/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
// Frame and strip geometry. Width 64, strip width 2/16 NDC == 4 px: instance i
// owns pixel columns [4i, 4i+4). All exact in binary floating point.
constexpr int kSize = 64;
constexpr int kStripPx = 4;
constexpr int kTol = 2; // LSB rounding across backends

// The CPU-side instance count the CSF pins ("INSTANCE_COUNT": "8"). If the
// indirect command were ignored, this is the strip count both phases would
// show.
constexpr int kCpuInstances = 8;

// The two GPU-written counts, both strictly below kCpuInstances so neither
// phase can be confused with the non-indirect draw.
constexpr int kCountA = 3;
constexpr int kCountB = 6;

// syn-indirect-count.cs's descriptor inputs: the 'count' long control is the
// only INPUTS entry and the geometry resource's write_only attributes create
// no geometry INPUT port (only the geometry outlet); a literal
// INDIRECT.COUNT creates no port either. So the count control is raw input
// port 0.
constexpr int kCountPort = 0;

// Frames pumped after each control change; the change is applied on the next
// update() (fixture contract: setControl between render() calls), and the
// indirect command is re-written by the compute pass every frame, so >= 2
// frames settle it and 4 leaves margin.
constexpr int kFrames = 4;

/// Drawn pixel of strip i: R=255 marker, G=i (buffer identity), B=i (draw
/// identity), A=255 -- syn-instance-index-color.fs's closed form.
std::array<uint8_t, 4> drawn_px(int i)
{
  return {255, uint8_t(i), uint8_t(i), 255};
}

struct IndirectResult
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  // Which engine path consumed the GPU-written command buffer.
  bool gpuIndirect = false;    // QRhi::DrawIndirect (cb.drawIndirect)
  bool cpuReadback = false;    // ReadBackNonUniformBuffer fallback
  ReadbackImage at3, at6;
};

/// One session: build CSF -> raster -> sink, create once, then
/// count=3 / render / read, count=6 / render / read.
/// Collect only; Catch2 macros run after run_in_gui_app returns (fixture
/// header contract).
IndirectResult run_indirect(score::gfx::GraphicsApi be)
{
  IndirectResult r;
  r.backend = backend_name(be);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    // Capability probe on a throwaway RenderState (same construction the
    // fixture's own backend probe uses). caps.drawIndirect is populated only
    // on Qt >= 6.12 builds (RenderList.cpp:1580-1583); ReadBackNonUniformBuffer
    // gates the engine's CPU fallback (RenderedRawRasterPipelineNode.cpp:2951).
    {
      auto st = score::gfx::createRenderState(be, QSize{16, 16}, nullptr);
      if(st && st->rhi)
      {
        // Caps::populate is not exported from the plugin; ask QRhi directly.
        // caps.drawIndirect is QRhi::DrawIndirect on Qt >= 6.12
        // (RenderList.cpp:1580-1583), which is what this mirrors.
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
        r.gpuIndirect = st->rhi->isFeatureSupported(QRhi::DrawIndirect);
#else
        r.gpuIndirect = false;
#endif
        r.cpuReadback
            = st->rhi->isFeatureSupported(QRhi::ReadBackNonUniformBuffer);
        st->destroy();
      }
      // A failed probe is left to p.create(be) below, which skips cleanly.
    }

    GfxPipeline p;

    const int csf = p.addIsf(corpus("syn-indirect-count.cs"));
    const int raster = p.addRaster(
        corpus("syn-instance-index-color.vs"), corpus("syn-instance-index-color.fs"));
    if(csf < 0 || raster < 0)
    {
      r.error = "node build failed: " + p.error();
      return;
    }

    auto* gout = p.geometryOut(csf, 0);
    auto* gin = p.geometryIn(raster, 0);
    if(!gout || !gin)
    {
      r.error = gout ? "raw-raster node has no Geometry input port"
                     : "CSF producer has no Geometry output port";
      return;
    }
    p.wire(gout, gin);

    const int sink = p.addSink({kSize, kSize});
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      r.backend = p.backend();
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    // Neither engine path can consume the GPU-written command here: the
    // engine warns and degrades (draws stale/no commands) by design, so
    // there is no count contract to assert. SKIP, don't fail.
    if(!r.gpuIndirect && !r.cpuReadback)
    {
      r.skipped = true;
      r.skip_reason = "neither QRhi::DrawIndirect (Qt >= 6.12 + hardware) nor "
                      "QRhi::ReadBackNonUniformBuffer is supported -- the "
                      "engine cannot honor a GPU-written indirect command on "
                      "this backend";
      return;
    }

    // Phase A: GPU-written instanceCount = 3, CPU-side instances stays 8.
    setControl(*p.isf(csf), kCountPort, ossia::value{kCountA});
    p.render(kFrames);
    r.at3 = p.readback(sink);

    // Phase B: GPU-written instanceCount = 6 -- same session, no rebuild,
    // just the control; the compute pass re-writes the command buffer.
    setControl(*p.isf(csf), kCountPort, ossia::value{kCountB});
    p.render(kFrames);
    r.at6 = p.readback(sink);
  });
  return r;
}

/// Assert one phase's frame: strips [0, count) carry their closed-form drawn
/// pixel; strips [count, kCpuInstances) match the never-drawn reference.
/// The upper half of that range is the heart of the test: those instances
/// exist CPU-side (instances == 8) and their translations are written every
/// frame -- only the GPU-written command's smaller instanceCount can keep
/// them off the screen. Sampled at interior columns (4i+1, 4i+2) and rows
/// {8, 32, 56} to stay off rasterization edges.
void check_phase(const ReadbackImage& img, int count, const char* phase)
{
  INFO("phase " << phase << " (GPU-written count=" << count << ")");
  REQUIRE(img.valid());
  REQUIRE(img.width == kSize);
  REQUIRE(img.height == kSize);

  // Never-drawn reference: strip 14 (x = 58), untouched at any count <= 8
  // (columns end at x = 32). The raster pass clears to transparent and the
  // sink composites over black; assert the marker's absence rather than an
  // exact composite so the stale-vs-clear distinction stays honest.
  const auto ref = img.at(14 * kStripPx + 2, kSize / 2);
  INFO(
      "never-drawn reference = (" << int(ref[0]) << "," << int(ref[1]) << ","
                                  << int(ref[2]) << "," << int(ref[3]) << ")");
  REQUIRE(int(ref[0]) < 255 - 4 * kTol);

  const int rows[3] = {8, kSize / 2, kSize - 8};
  for(int i = 0; i < kCpuInstances; ++i)
  {
    for(int dx = 1; dx <= 2; ++dx)
    {
      const int x = i * kStripPx + dx;
      for(int y : rows)
      {
        const auto px = img.at(x, y);
        INFO(
            "strip " << i << " pixel (" << x << "," << y << ") = (" << int(px[0])
                     << "," << int(px[1]) << "," << int(px[2]) << "," << int(px[3])
                     << ")");
        if(i < count)
        {
          // Present, with BOTH identities equal to i: G from the translation
          // buffer, B from the draw call's gl_InstanceIndex (firstInstance
          // is 0 in the command, so draw ids start at 0 on both paths).
          const auto want = drawn_px(i);
          INFO(
              "expected drawn (" << int(want[0]) << "," << int(want[1]) << ","
                                 << int(want[2]) << "," << int(want[3]) << ")");
          CHECK(near(px, want, kTol));
        }
        else
        {
          // Absent although the CPU-side geometry_spec says 8 instances and
          // this instance's translation is populated: only the GPU-written
          // instanceCount can be keeping it off the screen. A draw that
          // ignored the indirect buffer paints R=255 here and fails.
          CHECK(near(px, ref, kTol));
        }
      }
    }
  }
}
} // namespace

TEST_CASE(
    "GPU-written indirect draw command drives the drawn instance count",
    "[gfx][l3][raster][csf][instancing][indirect]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  const IndirectResult r = run_indirect(backend);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  INFO(
      "indirect consumption path: "
      << (r.gpuIndirect ? "GPU drawIndirect (CustomMesh.cpp:683-696)"
                        : "CPU readback fallback "
                          "(RenderedRawRasterPipelineNode.cpp:2947-2977)"));
  // An empty/short readback on a supported backend FAILS (img.valid() below),
  // never skips; a build/wiring failure fails here.
  REQUIRE(r.error.empty());

  // The two dispatches wrote DIFFERENT counts; each phase must show exactly
  // its own coverage (3 strips, then 6), never the CPU-side 8.
  check_phase(r.at3, kCountA, "A count=3");
  check_phase(r.at6, kCountB, "B count=6");

  // Belt-and-braces difference check: the exact strips phase B added (3..5)
  // must be present in B and absent in A -- the two dispatches produced
  // different, predicted coverage (spec P1-8 wording).
  for(int i = kCountA; i < kCountB; ++i)
  {
    const auto pxA = r.at3.at(i * kStripPx + 2, kSize / 2);
    const auto pxB = r.at6.at(i * kStripPx + 2, kSize / 2);
    INFO(
        "strip " << i << " phase-A R=" << int(pxA[0]) << " phase-B R="
                 << int(pxB[0]));
    CHECK(int(pxA[0]) < 255 - 4 * kTol); // absent at count 3
    CHECK(int(pxB[0]) > 255 - 2 * kTol); // drawn at count 6
  }
}
