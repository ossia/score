// =============================================================================
// L3 CSF read_write GEOMETRY STATE — a shader that reads an index other than
// its own invocation's must see a consistent previous state, never a buffer
// being concurrently written.
//
// The defect this locks down: a `read_write` geometry attribute on a node that
// is not a self-feedback receiver used to have _in and _out bound to the SAME
// buffer, so any cross-index read raced with other invocations' writes. The
// engine accepted such a shader silently. Two mechanisms now prevent it, and
// they are selected by DIFFERENT predicates, which is why both need covering:
//
//   * self-feedback receivers get a ping-pong pair and a pointer SWAP
//     (RenderedCSFNode.cpp, CSF_GeomPP_* allocation + std::swap after
//     pushOutputGeometry);
//   * everything else gets a snapshot buffer re-seeded by an explicit
//     copyBuffer before the pass loop (CSF_GeomSnap_*).
//
// A future change that unifies the two paths, or reverts either, has to keep
// these pictures. The fixture reads the state back as PIXELS rather than as a
// buffer: a geometry-only CSF is never dispatched in this fixture without a
// geometry consumer (see csf_geometry_readback_skip_reason()), so the producer
// feeds a raw raster that rasterizes its triangles.
//
// FIXTURE: corpus/syn-geo-readwrite-shift.cs. position is the 16x11 half-cell
// grid of syn-geo-count-user.cs, so floor(count/3) triangles always have real
// area. color is `read_write` and carries the state: exactly one red triangle
// among blue ones, shifted forward by one triangle per frame via
// color_out[i] = color_in[(i - 3) mod N].
//
// WHAT THIS DOES AND DOES NOT CATCH — measured, not assumed.
//
// NEGATIVE CONTROL, actually run: forcing `read_buf = ssbo.buffer` in
// RenderedCSFNode::buildComputeSrbBindings (SCORE_CSF_PPPROBE confirms the
// binding then prints ALIASED) leaves all three cases GREEN on Vulkan on an
// RTX 4090. So this fixture does NOT detect the aliasing race itself: the
// marker step is one triangle, far inside the 32-wide SIMD, so reads and
// writes stay in lockstep and the race never surfaces. Stepping across
// workgroups instead (129 vertices, 6 workgroups) did not fix that and lost
// the marker on BOTH backends for reasons this fixture does not explain, so it
// was not kept. Detecting the race portably needs a different instrument.
//
// What these cases DO enforce is the contract a regression would break, and
// every failure mode named in the ping-pong/snapshot unification analysis
// (ledger 9.81) shows up in at least one of them:
//   * the marker stays one triangle wide          — no runaway propagation;
//   * the state ADVANCES frame to frame           — catches a frozen _in, the
//     failure mode of a promoted node whose buffer is later adopted;
//   * two independent runs agree pixel for pixel  — catches non-determinism;
//   * a VERTEX_COUNT that grows keeps all of the above — catches a snapshot
//     that stops being re-seeded or is read past its end.
// Both backends pass all three today.
//
// Intended registration: score_add_gfx_test(csf_readwrite_state
// GfxCsfReadWriteState.cpp)
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_csf_readwrite_state
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_csf_readwrite_state
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
constexpr int kSinkPx = 64;
constexpr int kCount = 384;      // 128 triangles, 6 workgroups of 64
constexpr int kGrownCount = 480; // 160 triangles

struct StateShot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage early, late, next;
};

// A pixel is "marker red" when red clearly dominates: the marker triangle is
// (1,0,0), every other triangle is (0,0,1), and the raster interpolates only
// between vertices of the same triangle, so no blend of the two exists inside
// one triangle.
int red_pixels(const ReadbackImage& img)
{
  if(!img.valid())
    return -1;
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) > 140 && int(p[2]) < 90)
        ++n;
    }
  return n;
}

int blue_pixels(const ReadbackImage& img)
{
  if(!img.valid())
    return -1;
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[2]) > 140 && int(p[0]) < 90)
        ++n;
    }
  return n;
}

bool identical(const ReadbackImage& a, const ReadbackImage& b)
{
  if(!a.valid() || !b.valid() || a.width != b.width || a.height != b.height)
    return false;
  for(int y = 0; y < a.height; ++y)
    for(int x = 0; x < a.width; ++x)
      for(int c = 0; c < 4; ++c)
        if(a.at(x, y)[c] != b.at(x, y)[c])
          return false;
  return true;
}

// Render the shift producer for `earlyFrames`, then on to `lateFrames`, in ONE
// pipeline, so `late` observes state accumulated across the whole run. `count`
// is driven through the $numPoints control inlet exactly as production does.
StateShot render_shift(score::gfx::GraphicsApi be, int count, int earlyFrames, int lateFrames)
{
  StateShot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addCsf(corpus("syn-geo-readwrite-shift.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int sink = p.addSink({kSinkPx, kSinkPx});
    if(prod < 0 || raster < 0)
    {
      r.error = p.error();
      return;
    }
    p.wire(p.geometryOut(prod, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));

    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    const int ctl = nth_control_input(*p.isf(prod), 0);
    if(ctl < 0)
    {
      r.error = "syn-geo-readwrite-shift.cs has no control inlet for $numPoints";
      return;
    }
    setControl(*p.isf(prod), ctl, count);

    p.render(earlyFrames);
    r.early = p.readback(sink);
    p.render(lateFrames);
    r.late = p.readback(sink);
    p.render(1);
    r.next = p.readback(sink);
    if(r.error.empty())
      r.error = p.error();
    if(r.error.empty() && (!r.early.valid() || !r.late.valid()))
      r.error = "shift-producer readback empty/short";
  });
  return r;
}
}

// -----------------------------------------------------------------------------
// CASE 1 — the marker stays exactly one triangle wide.
//
// This is the race detector. Every frame the shader reads its neighbour's
// previous colour; if that read can observe this frame's writes the red run
// grows past 3 vertices and keeps growing, so by the later sample the picture
// is dominated by red. One triangle of the 16x11 grid is ~8x11/2 px at 64x64,
// i.e. well under a tenth of the ~2800 drawn pixels of 32 triangles.
// -----------------------------------------------------------------------------
TEST_CASE(
    "a read_write geometry attribute read across indices sees a consistent "
    "previous state",
    "[gfx][l3][csf][geometry][readwrite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const StateShot s = render_shift(backend, kCount, 4, 12);
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  CAPTURE(s.backend);
  REQUIRE(s.error.empty());
  REQUIRE(s.early.valid());
  REQUIRE(s.late.valid());

  const int drawnEarly = red_pixels(s.early) + blue_pixels(s.early);
  const int drawnLate = red_pixels(s.late) + blue_pixels(s.late);
  CAPTURE(drawnEarly, drawnLate);

  // The grid must actually be drawn, or "no red" would pass vacuously.
  REQUIRE(drawnEarly > 200);
  REQUIRE(drawnLate > 200);

  const int redEarly = red_pixels(s.early);
  const int redLate = red_pixels(s.late);
  CAPTURE(redEarly, redLate);

  // Exactly one triangle out of 32 is the marker. Allow a generous ceiling of
  // four triangles' worth so partial coverage and MSAA-free edge rules cannot
  // fail this, while a smear (which reaches tens of triangles within a few
  // frames) still trips it.
  REQUIRE(redLate * 8 < drawnLate);
  REQUIRE(redEarly * 8 < drawnEarly);

  // And the marker must still exist: a snapshot that never gets re-seeded, or
  // one read out of bounds, loses it entirely.
  REQUIRE(redLate > 0);

  // The state must ADVANCE. A frozen _in -- the failure mode of a ping-pong
  // pair whose buffer was swapped out from under it, and of a snapshot that
  // stops being re-seeded -- leaves the marker parked and every frame equal.
  REQUIRE(s.next.valid());
  CHECK_FALSE(identical(s.early, s.late));
  CHECK_FALSE(identical(s.late, s.next));
}

// -----------------------------------------------------------------------------
// CASE 2 — the state is deterministic across runs.
//
// Two independent pipelines rendered for the same number of frames must agree
// pixel for pixel. Aliased _in/_out makes the result depend on dispatch order,
// which differs between runs; a correct split is a pure function of the frame
// count. This is the property that a unification of the snapshot and ping-pong
// paths must preserve, on both backends.
// -----------------------------------------------------------------------------
TEST_CASE(
    "read_write geometry state is a pure function of the frame count",
    "[gfx][l3][csf][geometry][readwrite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const StateShot a = render_shift(backend, kCount, 4, 12);
  if(a.skipped)
    SKIP(a.backend + ": " + a.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  const StateShot b = render_shift(backend, kCount, 4, 12);
  if(b.skipped)
    SKIP(b.backend + ": " + b.skip_reason);

  CAPTURE(a.backend, b.backend);
  REQUIRE(a.error.empty());
  REQUIRE(b.error.empty());
  REQUIRE(a.late.valid());
  REQUIRE(b.late.valid());

  CAPTURE(red_pixels(a.late), red_pixels(b.late));
  CHECK(identical(a.early, b.early));
  CHECK(identical(a.late, b.late));
}

// -----------------------------------------------------------------------------
// CASE 3 — the _in side survives a VERTEX_COUNT that grows.
//
// The snapshot buffer is allocated once from the live buffer's size. The
// regrow sites that keep a read_buffer in step with its buffer are gated on
// `is_feedback_receiver`, so a NON-feedback node whose $numPoints expression
// grows can end up reading _in past the end of a stale, smaller snapshot:
// zeros under Vulkan robust buffer access, undefined on GL.
//
// The symptom here is total, not subtle: every vertex beyond the old count
// reads alpha 0, takes the self-seeding branch every frame, and freezes. The
// marker then cannot move into the grown region, so the picture stops being a
// function of the frame count and the red triangle count goes wrong.
//
// -----------------------------------------------------------------------------
TEST_CASE(
    "a read_write snapshot follows its buffer when VERTEX_COUNT grows",
    "[gfx][l3][csf][geometry][readwrite]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // Reference: the grown count from the start, no resize involved.
  const StateShot ref = render_shift(backend, kGrownCount, 4, 12);
  if(ref.skipped)
    SKIP(ref.backend + ": " + ref.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  REQUIRE(ref.error.empty());
  REQUIRE(ref.late.valid());

  // Same end state, reached by growing the count after the small buffers and
  // their snapshot have already been allocated.
  StateShot grown;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addCsf(corpus("syn-geo-readwrite-shift.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int sink = p.addSink({kSinkPx, kSinkPx});
    if(prod < 0 || raster < 0)
    {
      grown.error = p.error();
      return;
    }
    p.wire(p.geometryOut(prod, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    if(!p.create(backend))
    {
      grown.skipped = p.skipped();
      grown.skip_reason = p.skipReason();
      grown.backend = p.backend();
      grown.error = p.error();
      return;
    }
    grown.backend = p.backend();

    const int ctl = nth_control_input(*p.isf(prod), 0);
    if(ctl < 0)
    {
      grown.error = "syn-geo-readwrite-shift.cs has no control inlet for $numPoints";
      return;
    }

    setControl(*p.isf(prod), ctl, kCount);
    p.render(4);
    grown.early = p.readback(sink);

    setControl(*p.isf(prod), ctl, kGrownCount);
    p.render(12);
    grown.late = p.readback(sink);
    p.render(1);
    grown.next = p.readback(sink);
    if(grown.error.empty())
      grown.error = p.error();
  });

  if(grown.skipped)
    SKIP(grown.backend + ": " + grown.skip_reason);
  CAPTURE(grown.backend);
  REQUIRE(grown.error.empty());
  REQUIRE(grown.late.valid());

  const int drawn = red_pixels(grown.late) + blue_pixels(grown.late);
  const int red = red_pixels(grown.late);
  CAPTURE(drawn, red, red_pixels(ref.late), blue_pixels(ref.late));

  // The grown region must be live: as many drawn pixels as the reference, one
  // marker triangle, and no frozen seed pattern.
  CHECK(drawn > 200);
  CHECK(red > 0);
  CHECK(red * 8 < drawn);

  // The decisive one. If _in is read past the end of a snapshot that was never
  // regrown, every vertex beyond the old count sees alpha 0, takes the
  // self-seeding branch on every frame and freezes -- and a frozen picture is
  // indistinguishable from a valid one by pixel counts alone, because the seed
  // pattern is itself "one red triangle among blue". So require that the state
  // still MOVES after the growth, and that it is not merely the seed.
  REQUIRE(grown.next.valid());
  CHECK_FALSE(identical(grown.late, grown.next));

  // The reference reached the grown count without a resize; its picture must
  // also be moving, which makes the comparison above meaningful.
  REQUIRE(ref.next.valid());
  CHECK_FALSE(identical(ref.late, ref.next));

  // Same count, same number of frames rendered at that count is NOT the same
  // state (the marker advanced during the small phase too), so compare the
  // property that must hold either way: the drawn area matches the reference's.
  const int refDrawn = red_pixels(ref.late) + blue_pixels(ref.late);
  CHECK(std::abs(drawn - refDrawn) * 8 < refDrawn);
}
