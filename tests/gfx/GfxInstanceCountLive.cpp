// =============================================================================
// P0-1 -- INSTANCE COUNT CHANGED MID-RENDER redraws the right number of
// instances.
//
// Intended registration: score_add_gfx_test(instance_count_live GfxInstanceCountLive.cpp)
//
// A CSF compute shader (corpus/syn-instance-count-user.cs) declares
//   "INSTANCE_COUNT": "$USER"
// on its geometry resource and feeds a raw-raster consumer
// (corpus/syn-instance-index-color.{vs,fs}). Each instance is one quad
// covering NDC column [-1 + 0.125 i, -1 + 0.125 (i+1)] -- i.e. pixel columns
// [4i, 4i+4) of the 64x64 frame -- via a per-instance 'translation' attribute
// (RATE: instance), NOT MODEL_MATRIX. The fragment writes, per instance i:
//
//   R = 255            (drawn-coverage marker)
//   G = i  exactly     (translation.w == i/255 written by the PER_INSTANCE
//                       compute pass -- the identity stored IN the buffer
//                       that must be reallocated on a count change)
//   B = i  exactly     (gl_InstanceIndex/255 -- the identity of the DRAW
//                       CALL -- handed from the vertex stage through a flat
//                       int varying, since gl_InstanceIndex is vertex-only)
//   A = 255
//
// Both identities are exactly representable: round(255 * i/255) == i on the
// fixture's plain non-sRGB RGBA8 target. Undrawn area: the raster pass clears
// to Qt::transparent (RenderedRawRasterPipelineNode.cpp:3148-3151), so a
// never-drawn column cannot carry R == 255; the never-drawn reference pixel is
// sampled from column strip 14 (x = 58), which no phase (max count 9) reaches.
//
// The count control starts at 4, moves to 9 (grow) and then 2 (shrink) WITHIN
// THE SAME RENDER SESSION -- one GfxPipeline, one create(), setControl between
// render() calls, no graph rebuild. Strip count and both identity channels
// must track exactly, with no stale strip left from the previous count.
//
// ENGINE SURFACE DRIVEN (all verified in source, this worktree):
//  * "$USER" in INSTANCE_COUNT creates an int control port with default 1
//    (ISFNode.cpp:279-287) and a synthesized `geo_instance_count` int uniform
//    in the compute GLSL (libisf isf.cpp:4127-4128 / 5946-5949).
//  * resolveCountExpression registers var_USER from the port's CURRENT value
//    (`*(int*)port->value`, RenderedCSFNode.cpp:377-385) and is re-run by
//    updateGeometryBindings (RenderedCSFNode.cpp:1048-1052), which update()
//    calls every frame (RenderedCSFNode.cpp:4179) -- so a setControl between
//    render() calls IS re-evaluated with no rebuild.
//  * A changed count resizes the attribute SSBOs (elem_stride * count,
//    RenderedCSFNode.cpp:1647-1670 no-upstream branch; 1589-1624 with
//    upstream) and pushOutputGeometry's structural check
//    `binding.prev_instance_count != binding.instance_count`
//    (RenderedCSFNode.cpp:1782) forces the full output-geometry rebuild,
//    committing prev at RenderedCSFNode.cpp:2321.
//  * The PER_INSTANCE dispatch is sized from the binding's live
//    instance_count via TARGET (RenderedCSFNode.cpp:4519-4560).
//  * RATE "instance" attributes become per_instance vertex bindings with
//    step_rate 1 (RenderedCSFNode.cpp:1842-1845, 2233-2236; honoured by
//    CustomMesh.cpp:552-558) and the draw issues cb.draw(g.vertices,
//    g.instances) (CustomMesh.cpp:722-724).
// Given all of the above, the CORRECT behaviour asserted here is also the
// behaviour the source implements, so this is expected GREEN.
//
// ALLOC-COUNT ASSERTION DROPPED (honestly): the spec asks to count
// "CSF ALLOC [createStorageBuffer]" trace lines, but NO such string exists
// anywhere in src/plugins/score-plugin-gfx (verified by grep).
// RenderedCSFNode::createStorageBuffer (RenderedCSFNode.cpp:722) emits no
// trace at all -- only a qWarning on FAILURE -- and the no-upstream resize
// path (RenderedCSFNode.cpp:1647-1670) resizes buffers in place with no
// trace either. SCORE_GFX_TRACE gates only Graph.cpp/ImageNode.cpp/Window.cpp
// lines; the [BUFTRACE] channel (CustomMesh.cpp:15-22, on unless
// SCORE_BUFTRACE=0) covers CustomMesh::reload, whose per-change invocation
// count is not a specified contract (per-renderer, per-edge). No reliable
// closed-form line count exists, so no fake one is asserted. The reallocation
// is instead validated through its OBSERVABLE contract: the G channel is the
// content of the reallocated buffer, so a size change without a correct
// rewrite (stale or zeroed contents) breaks G while B survives.
//
// NEGATIVE CONTROL (one line, product side, for the orchestrator): in
// src/plugins/score-plugin-gfx/Gfx/Graph/RenderedCSFNode.cpp:1782 change
//   `|| binding.prev_instance_count != binding.instance_count`
// to `|| false` -- the output geometry then keeps its old instance count, the
// grow phase keeps drawing 4 strips where 9 are required, and this test's
// grow-phase "strip present" checks go red.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_instance_count_live
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_instance_count_live
// QT_QPA_PLATFORM=offscreen must NOT be used: the Null backend renders a
// stable, self-consistent and completely wrong picture; the verdict here is
// pixels, so unavailable backends SKIP instead.
// =============================================================================
#include "IsfTestCommon.hpp"

#include <ossia/network/value/value.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

namespace
{
// Frame and strip geometry. Width 64, strip width 2/16 NDC == 4 px: instance i
// owns pixel columns [4i, 4i+4). All exact in binary floating point.
constexpr int kSize = 64;
constexpr int kStripPx = 4;
constexpr int kMaxCount = 9; // largest count any phase uses
constexpr int kTol = 2;      // LSB rounding across backends

// The count sequence, all in ONE render session.
constexpr int kCountA = 4; // initial
constexpr int kCountB = 9; // grow
constexpr int kCountC = 2; // shrink

// Frames pumped after each control change; the change is applied on the next
// update() (fixture contract: setControl between render() calls), so >= 2
// frames settle it and 4 leaves margin.
constexpr int kFrames = 4;

// syn-instance-count-user.cs has exactly one descriptor input: the IntSpinBox
// port synthesized for the $USER INSTANCE_COUNT (ISFNode.cpp:279-287; the
// write_only geometry attributes create no geometry INPUT port, only the
// geometry output). So the count control is raw input port 0.
constexpr int kCountPort = 0;

/// Drawn pixel of strip i: R=255 marker, G=i (buffer identity), B=i (draw
/// identity), A=255. Exact -- see file header.
std::array<uint8_t, 4> drawn_px(int i)
{
  return {255, uint8_t(i), uint8_t(i), 255};
}

struct LiveResult
{
  bool skipped = false;
  std::string skip_reason;
  std::string error;
  std::string backend;
  ReadbackImage at4, at9, at2;
};

/// One session: build CSF -> raster -> sink, create once, then
/// count=4 / render / read, count=9 / render / read, count=2 / render / read.
/// Collect only; Catch2 macros run after run_in_gui_app returns (fixture
/// header contract).
LiveResult run_live(score::gfx::GraphicsApi be)
{
  LiveResult r;
  r.backend = backend_name(be);
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;

    const int csf = p.addIsf(corpus("syn-instance-count-user.cs"));
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

    // Phase A: count = 4. setControl drives ProcessNode::process(port, value),
    // the same public entry point the exec engine uses (Gfx.hpp setControl).
    setControl(*p.isf(csf), kCountPort, ossia::value{kCountA});
    p.render(kFrames);
    r.at4 = p.readback(sink);

    // Phase B: GROW to 9 -- same session, no rebuild, just the control.
    setControl(*p.isf(csf), kCountPort, ossia::value{kCountB});
    p.render(kFrames);
    r.at9 = p.readback(sink);

    // Phase C: SHRINK to 2 -- strips 2..8 from phase B must vanish.
    setControl(*p.isf(csf), kCountPort, ossia::value{kCountC});
    p.render(kFrames);
    r.at2 = p.readback(sink);
  });
  return r;
}

/// Assert one phase's frame: strips [0, count) carry their closed-form drawn
/// pixel; strips [count, kMaxCount) match the never-drawn reference (which is
/// what catches a stale strip: stale carries the R=255 marker, the clear does
/// not). Sampled at interior columns (4i+1, 4i+2) and rows {8, 32, 56} to
/// stay off rasterization edges.
void check_phase(const ReadbackImage& img, int count, const char* phase)
{
  INFO("phase " << phase << " (count=" << count << ")");
  REQUIRE(img.valid());
  REQUIRE(img.width == kSize);
  REQUIRE(img.height == kSize);

  // Never-drawn reference: strip 14, untouched at any count <= 9. The raster
  // pass clears to transparent and the sink composites over black, so this
  // cannot carry the drawn marker -- assert that instead of assuming the
  // exact composite, so the stale-vs-clear distinction stays honest.
  const auto ref = img.at(14 * kStripPx + 2, kSize / 2);
  INFO(
      "never-drawn reference = (" << int(ref[0]) << "," << int(ref[1]) << ","
                                  << int(ref[2]) << "," << int(ref[3]) << ")");
  REQUIRE(int(ref[0]) < 255 - 4 * kTol);

  const int rows[3] = {8, kSize / 2, kSize - 8};
  for(int i = 0; i < kMaxCount; ++i)
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
          // Present, with BOTH identities equal to i: G from the reallocated
          // buffer's contents, B from the draw call's gl_InstanceIndex.
          const auto want = drawn_px(i);
          INFO(
              "expected drawn (" << int(want[0]) << "," << int(want[1]) << ","
                                 << int(want[2]) << "," << int(want[3]) << ")");
          CHECK(near(px, want, kTol));
        }
        else
        {
          // Absent: must equal the never-drawn reference. A strip left over
          // from the previous count still carries R=255 and fails here.
          CHECK(near(px, ref, kTol));
        }
      }
    }
  }
}
} // namespace

TEST_CASE(
    "CSF user-driven instance count tracks grow and shrink mid-session",
    "[gfx][l3][raster][csf][instancing]")
{
  const auto backend = GENERATE(from_range(platform_backends()));

  const LiveResult r = run_live(backend);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(why);
  INFO("backend=" << r.backend);
  // An empty/short readback on a supported backend FAILS (img.valid() below),
  // never skips; a build/wiring failure fails here.
  REQUIRE(r.error.empty());

  check_phase(r.at4, kCountA, "A initial");
  check_phase(r.at9, kCountB, "B grow");
  check_phase(r.at2, kCountC, "C shrink");

  // Belt-and-braces stale check across phases: the exact columns phase B
  // added (4..8) must be gone again in phase C, i.e. differ from their
  // phase-B drawn value by more than tolerance somewhere obvious (R marker).
  for(int i = kCountC; i < kCountB; ++i)
  {
    const auto pxB = r.at9.at(i * kStripPx + 2, kSize / 2);
    const auto pxC = r.at2.at(i * kStripPx + 2, kSize / 2);
    INFO(
        "strip " << i << " phase-B R=" << int(pxB[0]) << " phase-C R="
                 << int(pxC[0]));
    CHECK(int(pxB[0]) > 255 - 2 * kTol); // was drawn in B
    CHECK(int(pxC[0]) < 255 - 4 * kTol); // gone in C
  }
}
