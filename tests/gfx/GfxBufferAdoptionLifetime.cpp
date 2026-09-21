// =============================================================================
// A consumer that adopted a producer's buffer must not keep the pointer after
// the producer retires it.
//
// A CSF adopts an upstream buffer with owned=false and stores the raw
// QRhiBuffer*. The shader-resource-binding rebuild is gated on a hash of those
// pointers, so a pointer that goes dead without changing hashes the same and
// the SRB is never rebuilt. Qt does not catch it: its generation check reads
// m_id off the resource, and sanityCheckResourceOwnership dereferences
// m_rhi->q to find out -- which is the SIGSEGV. Whether it crashes depends on
// whether the freed chunk has been reused, so the bug shows up intermittently.
//
// RenderList::checkBindingsLive gives a liveness oracle that reads only the
// binding list, never the buffer objects, and SCORE_ASSERT throws in release
// builds, so these tests fail deterministically rather than one run in three.
//
// Registration:
//   score_add_gfx_test(buffer_adoption_lifetime GfxBufferAdoptionLifetime.cpp)
//
//   DISPLAY=:0 SCORE_TESTS_NO_XVFB=1 SCORE_TEST_API=vulkan \
//     ctest -R gfx_buffer_adoption_lifetime
#include "GfxIncrementalCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
// A CSF chain whose consumer adopts the producer's geometry buffers, drawn by
// a raw raster so the geometry actually reaches a pipeline.
struct Chain
{
  int hub{-1}, step{-1}, tap{-1}, raster{-1}, sink{-1};
  bool ok() const noexcept
  {
    return hub >= 0 && step >= 0 && tap >= 0 && raster >= 0 && sink >= 0;
  }
};

Chain build(GfxPipeline& p)
{
  Chain c;
  c.hub = p.addCsf(corpus("syn-loop-hub.cs"));
  c.step = p.addCsf(corpus("syn-loop-step.cs"));
  c.tap = p.addCsf(corpus("syn-loop-tap.cs"));
  c.raster
      = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
  c.sink = p.addSink({64, 64});
  if(!c.ok())
    return c;
  p.wire(p.geometryOut(c.hub, 0), p.geometryIn(c.step, 0));
  p.wire(p.geometryOut(c.step, 0), p.geometryIn(c.tap, 0));
  p.wire(p.geometryOut(c.tap, 0), p.geometryIn(c.raster, 0));
  p.wire(p.imageOut(c.raster, 0), p.sinkInput(c.sink));
  return c;
}
}

// The ordering the corpus crashes on: the consumers adopt first, and only then
// does the producer become a feedback receiver. The adoption-skip branches for
// a feedback receiver keep whatever was adopted before the promotion, so a
// buffer retired during the switch stays bound.
TEST_CASE(
    "a feedback edge closed after adoption leaves no retired buffer bound",
    "[gfx][l3][csf][lifetime]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  // compute_shader_skip_reason builds a GL capability probe, which needs a
  // live QGuiApplication: called from the bare test body it takes
  // QOffscreenSurface::create() down (the Mesa offscreen GLX fault).
  std::string skip;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      skip = std::string{backend_name(api)} + ": " + why;
      return;
    }
    GfxPipeline p;
    const auto c = build(p);
    REQUIRE(c.ok());
    REQUIRE(p.create(api));

    // Frames enough for the consumers to adopt the producer's handles.
    p.render(8);

    // Close the loop on the running graph: hub becomes a feedback receiver
    // while step and tap already hold its buffers.
    p.addEdgeIncremental(
        p.geometryOut(c.tap, 0), p.geometryIn(c.hub, 0),
        Process::CableType::DelayedGlutton);

    // Every frame here submits the compute SRBs. If a consumer kept a pointer
    // the promotion retired, checkBindingsLive fails inside the render.
    p.render(24);
    REQUIRE(p.error().empty());
  });
  if(!skip.empty())
    SKIP(skip);
}

// The same invariant under the reverse edit: the producer keeps rendering but
// a consumer's upstream edge goes away, so the attribute it adopted is no
// longer published. The fallback branch must not keep a borrowed pointer.
TEST_CASE(
    "dropping a producer edge leaves no retired buffer bound",
    "[gfx][l3][csf][lifetime]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  // compute_shader_skip_reason builds a GL capability probe, which needs a
  // live QGuiApplication: called from the bare test body it takes
  // QOffscreenSurface::create() down (the Mesa offscreen GLX fault).
  std::string skip;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      skip = std::string{backend_name(api)} + ": " + why;
      return;
    }
    GfxPipeline p;
    const auto c = build(p);
    REQUIRE(c.ok());
    REQUIRE(p.create(api));

    p.render(8);
    p.removeEdgeIncremental(p.geometryOut(c.hub, 0), p.geometryIn(c.step, 0));
    p.render(24);
    REQUIRE(p.error().empty());
  });
  if(!skip.empty())
    SKIP(skip);
}

// Re-adding the edge re-publishes handles that may differ from the ones the
// consumer adopted before the removal.
TEST_CASE(
    "re-adding a producer edge rebinds rather than keeping the old handles",
    "[gfx][l3][csf][lifetime]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(api));
  // compute_shader_skip_reason builds a GL capability probe, which needs a
  // live QGuiApplication: called from the bare test body it takes
  // QOffscreenSurface::create() down (the Mesa offscreen GLX fault).
  std::string skip;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    if(const char* why = compute_shader_skip_reason(api))
    {
      skip = std::string{backend_name(api)} + ": " + why;
      return;
    }
    GfxPipeline p;
    const auto c = build(p);
    REQUIRE(c.ok());
    REQUIRE(p.create(api));

    p.render(8);
    p.removeEdgeIncremental(p.geometryOut(c.hub, 0), p.geometryIn(c.step, 0));
    p.render(4);
    p.addEdgeIncremental(p.geometryOut(c.hub, 0), p.geometryIn(c.step, 0));
    p.render(24);
    REQUIRE(p.error().empty());
  });
  if(!skip.empty())
    SKIP(skip);
}
