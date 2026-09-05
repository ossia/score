// =============================================================================
// L3 GEOMETRY-BUFFER LIFETIME — a geometry buffer released on the frame it is
// rebound does not dangle (SPEC P0-8).
//
// Geometry-buffer sibling of GfxBatchLifetime.cpp (material UBOs, "Do NOT
// weaken"). The defect class: QRhiResourceUpdateBatch stores raw QRhiBuffer*
// until commit, so a buffer that is released while the current frame's batch
// still holds a pending uploadStaticBuffer naming it dangles at submit time.
// RenderList::releaseBuffer() defends against exactly that with a deliberate
// deleteLater() (RenderList.cpp:523-550; the comment at :545-548 — "the buffer
// may still be referenced by pending uploadStaticBuffer operations in the
// current frame's batch" — and the deleteLater() itself at :549, which defers
// native destruction past the frame; outside a frame, Qt deletes immediately,
// which is why the incremental mutation path must submit the pending batch
// before any teardown, the fix GfxBatchLifetime pins).
//
// Engine facts this file keys on (verified in this worktree):
//  * A CSF geometry producer with an expression VERTEX_COUNT resolves the
//    count from its own control inlet each update: resolveCountExpression
//    (RenderedCSFNode.cpp:290) registers a long input's live value as
//    var_<name> from *(int*)port->value (RenderedCSFNode.cpp:~505-511), the
//    same slot the fixture's setControl() drives through
//    ProcessNode::process(int32_t, ossia::value).
//  * On a resolved-count change, the producer reallocates its owned attribute
//    SSBOs and queues a zero-fill res.uploadStaticBuffer into the CURRENT
//    frame's batch (standalone "CSF_GeomSpec_" resize block,
//    RenderedCSFNode.cpp:1663-1686; upstream-fed "CSF_Geom_" reallocation via
//    renderer.releaseBuffer + newBuffer at RenderedCSFNode.cpp:1386-1395).
//  * pushOutputGeometry detects the count change as a structural change
//    (prev_vertex_count / prev_instance_count comparison,
//    RenderedCSFNode.cpp:1779-1786; latched at :2320-2321), releases escaped
//    COPY_FROM buffers through renderer.releaseBuffer (:1789-1791) and
//    republishes a NEW mesh_list to the downstream renderer.
//  * The downstream raw-raster consumer sees geometryChanged and rebinds
//    through RenderList::acquireMesh IN THE SAME FRAME
//    (RenderedRawRasterPipelineNode.cpp:2435-2441; acquireMesh re-key path,
//    RenderList.cpp:684+).
//  * At incremental teardown, RenderedCSFNode::release() hands every owned
//    geometry buffer to RenderList::releaseBuffer
//    (RenderedCSFNode.cpp:4012-4036) — while the pending initial batch may
//    still hold the init-time zero-fill uploadStaticBuffer queued for those
//    same buffers when the renderer was created
//    (init pre-allocation "CSF_GeomSpec_" + upload,
//    RenderedCSFNode.cpp:3694-3702).
//
// FIXTURE NOTE (orchestrator): the producer shader is syn-geo-count-user.cs,
// written for this test. The first draft reused csf-vertex-count-expr.cs,
// whose spiral lies almost entirely OUTSIDE the viewport: whether any sample
// hits a visible pixel depends non-monotonically on the count (measured:
// 64/65/128/129/160/224/240 draw pixels, 192/255/256/448/512 draw none), so
// the oracle went blank at the spec'd counts. The new shader tiles the
// viewport with one real-area triangle per 3 vertices, so every legal count
// rasterizes floor(count/3) triangles and both closed forms are non-blank.
//
// CASE 1 drives the reallocate-and-rebind frame: producer(count $numPoints) ->
// raw-raster -> sink; flip the count 64 -> 512 -> 64 -> 512 between frames.
// Every rendered frame must be one of the two closed forms (the fixed-count
// pictures captured from independent fresh pipelines) — never garbage, never a
// stale mix — and the steady frame after each change must match the NEW size's
// closed form. Use-after-free itself is the ASan run's verdict; this makes the
// dangerous sequence happen deterministically.
//
// CASE 2 is the literal geometry twin of GfxBatchLifetime: with NO render in
// between, incrementally add a second raster + geometry producer B (leaves B's
// CSF_GeomSpec_ zero-fill uploads pending in the initial batch), tear B (and
// the helper raster) down through the same incremental path the app uses, and
// render again. The pending batch must be submitted before B's buffers are
// released; the sink must still show the original producer's picture.
//
// NEGATIVE CONTROL (product-side, do not commit): in RenderList::releaseBuffer
// (src/plugins/score-plugin-gfx/Gfx/Graph/RenderList.cpp:523), replace
//   buf->deleteLater();            // RenderList.cpp:549
// with
//   delete buf;
// Under ASan this is a heap-use-after-free when the frame's batch commits the
// pending uploadStaticBuffer that still names the buffer (queued at
// RenderedCSFNode.cpp:3700 / :1267 / :1667-1686), reached through
// RenderedCSFNode::release() -> releaseBuffer (RenderedCSFNode.cpp:4019) and
// the update-path release sites (:1259, :1347, :1388, :1519, :1546, :1790).
//
// Intended registration: score_add_gfx_test(geometry_buffer_rebind
// GfxGeometryBufferRebind.cpp)
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_geometry_buffer_rebind
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_geometry_buffer_rebind
// =============================================================================
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
// Per-channel tolerance for "same picture" within one backend (rasterized
// point clouds are deterministic per backend; GL-vs-Vulkan agreement is
// GfxRaster.cpp's job, not this file's).
constexpr int kTol = 2;

constexpr int kSmallCount = 64;  // syn-geo-count-user.cs default (21 triangles)
constexpr int kLargeCount = 512; // its declared MAX — 1 KiB -> 8 KiB per attr

int rebind_max_channel_diff(const ReadbackImage& a, const ReadbackImage& b)
{
  if(!a.valid() || !b.valid() || a.width != b.width || a.height != b.height)
    return 256;
  int worst = 0;
  for(int y = 0; y < a.height; ++y)
    for(int x = 0; x < a.width; ++x)
    {
      const auto pa = a.at(x, y);
      const auto pb = b.at(x, y);
      for(int c = 0; c < 4; ++c)
        worst = std::max(worst, std::abs(int(pa[c]) - int(pb[c])));
    }
  return worst;
}

int drawn_pixels(const ReadbackImage& img, int thresh = 12)
{
  if(!img.valid())
    return 0;
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) + int(p[1]) + int(p[2]) > thresh)
        ++n;
    }
  return n;
}

bool same_picture(const ReadbackImage& a, const ReadbackImage& b)
{
  return rebind_max_channel_diff(a, b) <= kTol;
}

// One fixed-count closed form: a FRESH producer -> raster -> sink pipeline
// with the count pinned before any frame, rendered to steady state. The
// mutation run's frames are compared against these mutation-free pictures, so
// a stale-buffer artifact (old-count points bleeding into the new picture)
// cannot hide in a self-referencing oracle.
struct FixedShot
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  ReadbackImage img;
};

FixedShot render_fixed(score::gfx::GraphicsApi be, int count)
{
  FixedShot r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addCsf(corpus("syn-geo-count-user.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int s0 = p.addSink({64, 64});
    if(prod < 0 || raster < 0)
    {
      r.error = p.error();
      return;
    }
    p.wire(p.geometryOut(prod, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(s0));

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
      r.error = "syn-geo-count-user.cs has no control inlet for $numPoints";
      return;
    }
    setControl(*p.isf(prod), ctl, count);

    p.render(4);
    r.img = p.readback(s0);
    r.error = p.error();
    if(r.error.empty() && !r.img.valid())
      r.error = "fixed-count readback empty/short";
  });
  return r;
}
}

// -----------------------------------------------------------------------------
// CASE 1 — the count flips between frames; on each transition frame the
// producer reallocates its geometry SSBOs, republishes a new mesh_list, and
// the raster rebinds it, all inside ONE RenderList::render(). Every frame the
// sink shows must be exactly one of the two closed forms.
// -----------------------------------------------------------------------------
TEST_CASE(
    "geometry buffer reallocated by a count change on the frame the raster "
    "rebinds it",
    "[gfx][l3][raster][geobuf][batchlife]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // Closed forms from mutation-free pipelines.
  const FixedShot refSmall = render_fixed(backend, kSmallCount);
  if(refSmall.skipped)
    SKIP(refSmall.backend + ": " + refSmall.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(why);
  INFO("ref small: backend=" << refSmall.backend << " error=" << refSmall.error);
  REQUIRE(refSmall.error.empty());
  REQUIRE(refSmall.img.valid());

  const FixedShot refLarge = render_fixed(backend, kLargeCount);
  if(refLarge.skipped)
    SKIP(refLarge.backend + ": " + refLarge.skip_reason);
  INFO("ref large: backend=" << refLarge.backend << " error=" << refLarge.error);
  REQUIRE(refLarge.error.empty());
  REQUIRE(refLarge.img.valid());

  // The oracle is only meaningful if the count is actually driveable and the
  // two counts rasterize differently (GfxRaster.cpp proves count 64 draws
  // blue-dominant points; 512 fills the visible slivers more densely).
  INFO(
      "drawn small=" << drawn_pixels(refSmall.img)
                     << " large=" << drawn_pixels(refLarge.img)
                     << " diff=" << rebind_max_channel_diff(refSmall.img, refLarge.img));
  REQUIRE(drawn_pixels(refSmall.img) > 0);
  REQUIRE(drawn_pixels(refLarge.img) > 0);
  REQUIRE(rebind_max_channel_diff(refSmall.img, refLarge.img) > 2 * kTol);

  // Mutation run: one pipeline, count flipped between render() calls.
  struct
  {
    bool skipped = false;
    std::string skip_reason, backend, error;
    ReadbackImage steady0;              // steady at 64 before any change
    ReadbackImage t1, steady1;          // 64 -> 512: transition, steady
    ReadbackImage t2, steady2;          // 512 -> 64: transition, steady
    ReadbackImage t3, steady3;          // 64 -> 512 again
    bool ran = false;
  } out;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addCsf(corpus("syn-geo-count-user.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int s0 = p.addSink({64, 64});
    if(prod < 0 || raster < 0)
    {
      out.error = p.error();
      return;
    }
    p.wire(p.geometryOut(prod, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(s0));

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    const int ctl = nth_control_input(*p.isf(prod), 0);
    if(ctl < 0)
    {
      out.error = "syn-geo-count-user.cs has no control inlet for $numPoints";
      return;
    }

    setControl(*p.isf(prod), ctl, kSmallCount);
    p.render(4);
    out.steady0 = p.readback(s0);

    // 64 -> 512: the next render()'s single frame resolves the new count,
    // reallocates the SSBOs, zero-fills them through the frame batch,
    // republishes the mesh and rebinds the raster — the P0-8 frame.
    setControl(*p.isf(prod), ctl, kLargeCount);
    p.render(1);
    out.t1 = p.readback(s0);
    p.render(2);
    out.steady1 = p.readback(s0);

    // 512 -> 64: shrink is a reallocation too (size != needed, not <).
    setControl(*p.isf(prod), ctl, kSmallCount);
    p.render(1);
    out.t2 = p.readback(s0);
    p.render(2);
    out.steady2 = p.readback(s0);

    // And grow again, so the sequence realloc/rebind happens three times.
    setControl(*p.isf(prod), ctl, kLargeCount);
    p.render(1);
    out.t3 = p.readback(s0);
    p.render(2);
    out.steady3 = p.readback(s0);

    out.error = p.error();
    out.ran = true;
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty()); // (a) no pipeline error across the size changes
  REQUIRE(out.ran);

  // (b) steady frames match the closed form for the CURRENT size.
  REQUIRE(out.steady0.valid());
  CHECK(same_picture(out.steady0, refSmall.img));
  REQUIRE(out.steady1.valid());
  CHECK(same_picture(out.steady1, refLarge.img));
  REQUIRE(out.steady2.valid());
  CHECK(same_picture(out.steady2, refSmall.img));
  REQUIRE(out.steady3.valid());
  CHECK(same_picture(out.steady3, refLarge.img));

  // (c) no rendered frame — transition frames included — is anything but one
  // of the two closed forms. A garbage frame (stale buffer contents, a
  // partial mix, uninitialized memory) matches neither.
  for(const ReadbackImage* t : {&out.t1, &out.t2, &out.t3})
  {
    REQUIRE(t->valid());
    INFO(
        "transition diff vs small=" << rebind_max_channel_diff(*t, refSmall.img)
                                    << " vs large="
                                    << rebind_max_channel_diff(*t, refLarge.img));
    CHECK((same_picture(*t, refSmall.img) || same_picture(*t, refLarge.img)));
  }
}

// -----------------------------------------------------------------------------
// CASE 2 — geometry twin of GfxBatchLifetime: a geometry producer is torn down
// through the incremental path while its init-time CSF_GeomSpec_ zero-fill
// uploads are still pending in the initial batch. Pre-fix for the material-UBO
// flavour this was a heap-use-after-free inside the backend's
// enqueueResourceUpdates on the very next frame; the geometry buffers ride the
// same batch-submission-before-teardown guarantee, and releaseBuffer's
// deleteLater() covers the in-frame release sites.
// -----------------------------------------------------------------------------
TEST_CASE(
    "geometry producer removed while its init geometry uploads are still "
    "batched",
    "[gfx][l3][raster][geobuf][batchlife]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Shot out;
  bool ran = false;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int prod = p.addCsf(corpus("syn-geo-count-user.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int s0 = p.addSink({64, 64});
    if(prod < 0 || raster < 0)
    {
      out.error = p.error();
      return;
    }
    p.wire(p.geometryOut(prod, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(s0));

    if(!p.create(backend))
    {
      out.skipped = p.skipped();
      out.skip_reason = p.skipReason();
      out.backend = p.backend();
      out.error = p.error();
      return;
    }
    out.backend = p.backend();

    p.render(4);
    out.a = p.readback(s0);

    // Inter-frame window, NO render inside it:
    //  1. add a second raster reachable from the sink (its renderer is
    //     created by reconcile),
    //  2. add geometry producer B feeding it — reconcile runs B's initState,
    //     which pre-allocates B's CSF_GeomSpec_ SSBOs and queues their
    //     zero-fill uploadStaticBuffer into the pending initial batch
    //     (RenderedCSFNode.cpp:3694-3702),
    //  3. tear B down through the app's own incremental removal —
    //     RenderedCSFNode::release() hands those same buffers to
    //     RenderList::releaseBuffer (RenderedCSFNode.cpp:4012-4036) while the
    //     batch still names them,
    //  4. remove the helper raster so the graph returns to its original
    //     shape before the next frame.
    const int raster2
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int b = p.addCsf(corpus("syn-geo-count-user.cs"));
    if(raster2 < 0 || b < 0)
    {
      out.error = p.error();
      return;
    }
    p.addEdgeIncremental(p.imageOut(raster2, 0), p.sinkInput(s0));
    p.addEdgeIncremental(p.geometryOut(b, 0), p.geometryIn(raster2, 0));

    p.removeNodeIncremental(b);
    p.removeNodeIncremental(raster2);

    // Pre-fix analog: the next frame submits the batch and writes through the
    // dangling pointers (hard crash under ASan before the readback).
    p.render(2);
    out.b = p.readback(s0);
    out.error = p.error();
    ran = true;
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(why);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(ran);

  REQUIRE(out.a.valid());
  INFO("baseline drawn=" << drawn_pixels(out.a));
  CHECK(drawn_pixels(out.a) > 0); // the spiral rasterized, not a blank clear

  // Post-mutation frame rendered and still shows the original producer's
  // picture, pixel for pixel.
  REQUIRE(out.b.valid());
  INFO("post-teardown diff=" << rebind_max_channel_diff(out.a, out.b));
  CHECK(same_picture(out.a, out.b));
}
