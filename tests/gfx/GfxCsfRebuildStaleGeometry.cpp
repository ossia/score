// A render-list rebuild must not leave a CSF node binding the geometry buffers
// its upstream published before the rebuild.
//
// A full rebuild of a render list (RenderList::maybeRebuild) runs release() +
// init() on the SAME renderer objects. Every CSF producer frees its geometry
// buffers in release(); a consumer that kept the upstream
// geometry_spec from before the rebuild rebinds those freed handles from its
// init(), and its compute pass is submitted with them before the producer
// publishes its new buffers: a use-after-free in setShaderResources, preceded
// by the "binds buffer ... which was retired" diagnostics. A resize of the
// output rebuilds only what follows the output size, which here is the raster
// alone; the chain must survive both.
//
// The graph is the reaction-diffusion corpus score's: a hub owning the grid
// (VERTEX_COUNT expression), diffusion, reaction closing a delayed geometry
// loop back to the hub, a colour map, and a raw raster drawing the points.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_csf_rebuild_stale_geometry
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_csf_rebuild_stale_geometry
#include "GfxIncrementalCommon.hpp"

#include <Gfx/Graph/RenderList.hpp>

using namespace score::test::gfx;
using namespace score::test::gfx::isf;
using namespace score::test::gfx::incremental;

namespace
{
struct RebuildRun
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  int staleBeforeResize = -1;
  int staleAfterResize = -1;
  int staleAfterRebuild = -1;
  ReadbackImage before, reseeded;
};

int drawn_pixels(const ReadbackImage& img)
{
  if(!img.valid())
    return 0;
  int n = 0;
  for(int y = 0; y < img.height; ++y)
    for(int x = 0; x < img.width; ++x)
    {
      const auto p = img.at(x, y);
      if(int(p[0]) + int(p[1]) + int(p[2]) > 8)
        ++n;
    }
  return n;
}

// Inside a frame, as the render loop rebuilds a list: what release() hands to
// deleteLater() outlives the rebuild, so no new resource takes its address.
void rebuildRenderLists(GfxPipeline& p)
{
  for(auto& rl : p.graph().renderLists())
  {
    score::gfx::OffscreenFrame frame{*rl->state.rhi};
    rl->maybeRebuild(true);
  }
}

RebuildRun run(score::gfx::GraphicsApi be)
{
  RebuildRun r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    const int init = p.addCsf(corpus("rd-init.cs"));
    const int diffuse = p.addCsf(corpus("rd-diffuse.cs"));
    const int react = p.addCsf(corpus("rd-react.cs"));
    const int display = p.addCsf(corpus("rd-display.cs"));
    const int raster
        = p.addRaster(corpus("raw-raster-basic.vs"), corpus("raw-raster-basic.fs"));
    const int sink = p.addSink({64, 64});
    if(init < 0 || diffuse < 0 || react < 0 || display < 0 || raster < 0)
    {
      r.error = p.error();
      return;
    }
    p.wire(p.geometryOut(init, 0), p.geometryIn(diffuse, 0));
    p.wire(p.geometryOut(diffuse, 0), p.geometryIn(react, 0));
    p.wire(p.geometryOut(react, 0), p.geometryIn(display, 0));
    p.wire(p.geometryOut(display, 0), p.geometryIn(raster, 0));
    p.wire(p.imageOut(raster, 0), p.sinkInput(sink));
    p.wireFeedback(p.geometryOut(react, 0), p.geometryIn(init, 0));

    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.backend = p.backend();
      r.error = p.error();
      return;
    }
    r.backend = p.backend();

    score::gfx::RenderList::resetStaleBindingCount();
    p.render(8);
    r.before = p.readback(sink);
    r.staleBeforeResize = score::gfx::RenderList::staleBindingCount();

    // Twice: the second rebuild starts from buffers the first one allocated.
    for(QSize sz : {QSize{48, 48}, QSize{64, 64}})
    {
      p.resizeSink(sink, sz);
      p.render(6);
    }
    r.staleAfterResize = score::gfx::RenderList::staleBindingCount();
    for(int i = 0; i < 2; ++i)
    {
      rebuildRenderLists(p);
      p.render(6);
    }

    // The rebuild gives every node new, zeroed buffers, and the hub only seeds
    // them on its first frames: the grid is empty until it is reset. Reseeding
    // shows the whole chain draws from the buffers allocated by the rebuild.
    setControl(*p.isf(init), 1, ossia::impulse{});
    p.render(8);
    r.reseeded = p.readback(sink);
    r.staleAfterRebuild = score::gfx::RenderList::staleBindingCount();
    if(r.error.empty())
      r.error = p.error();
  });
  return r;
}
}

TEST_CASE(
    "A CSF geometry chain survives a render-list rebuild without stale bindings",
    "[gfx][l3][csf][geometry][feedback][resize]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const RebuildRun s = run(backend);
  if(s.skipped)
    SKIP(s.backend + ": " + s.skip_reason);
  if(const char* why = compute_shader_skip_reason(backend))
    SKIP(std::string{backend_name(backend)} + ": " + why);
  CAPTURE(s.backend);
  REQUIRE(s.error.empty());

  // The fixture draws before the rebuild, so an empty frame after the reseed is
  // the rebuild's doing.
  REQUIRE(s.before.valid());
  CHECK(drawn_pixels(s.before) > 10);
  CHECK(s.staleBeforeResize == 0);

  CHECK(s.staleAfterResize == 0);
  CHECK(s.staleAfterRebuild == 0);
  REQUIRE(s.reseeded.valid());
  CHECK(drawn_pixels(s.reseeded) > 10);
}
