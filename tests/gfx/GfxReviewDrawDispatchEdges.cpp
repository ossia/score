// D1/D2 from the 2026-09 graphics review (section 4): indirect-draw edges.
//
// Two things nothing else in the suite covers, each driven through BOTH the GPU
// rung and the CPU fallback rung (SCORE_GFX_NO_GPU_INDIRECT /
// SCORE_GFX_NO_GPU_DISPATCH_INDIRECT), so the two must agree:
//
//   DrawDispatch-1  a non-indexed indirect command's firstInstance must select
//                   the same instances on every rung. Only strips 3 and 4 may
//                   be lit; anything else lit means the command ABI shifted.
//   DrawDispatch-2  a layered indirect dispatch must keep its volume slices.
//
// The review measured these out of tree and never registered them.
#include <score_test/Gfx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <cstdio>

using namespace score::test::gfx;
namespace {
QString artifact(const char* name)
{
  return QString{GFX_TEST_CORPUS_DIR "/"} + QString::fromUtf8(name);
}
QString corpus(const char* name) { return QString{GFX_TEST_CORPUS_DIR "/"} + name; }
struct Shot { bool skipped{}; std::string why, error; ReadbackImage image; };
Shot firstInstance(score::gfx::GraphicsApi api, bool cpu, bool abiControl)
{
  Shot result;
  const auto old = qgetenv("SCORE_GFX_NO_GPU_INDIRECT");
  if(cpu) qputenv("SCORE_GFX_NO_GPU_INDIRECT", "1");
  else qunsetenv("SCORE_GFX_NO_GPU_INDIRECT");
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    int csf = p.addIsf(artifact(abiControl ? "DrawDispatch_first_instance_abi_control.cs"
                                         : "DrawDispatch_first_instance.cs"));
    int raster = p.addRaster(corpus("syn-instance-index-color.vs"),
                             corpus("syn-instance-index-color.fs"));
    if(csf < 0 || raster < 0) { result.error = p.error(); return; }
    auto* out = p.geometryOut(csf, 0);
    auto* in = p.geometryIn(raster, 0);
    if(!out || !in) { result.error = "Missing geometry port"; return; }
    p.wire(out, in);
    int sink = p.addSink({64,64});
    p.wire(p.imageOut(raster,0), p.sinkInput(sink));
    if(!p.create(api)) { result.skipped = p.skipped(); result.why = p.skipReason(); result.error = p.error(); return; }
    p.render(3);
    result.image = p.readback(sink);
  });
  if(old.isNull()) qunsetenv("SCORE_GFX_NO_GPU_INDIRECT"); else qputenv("SCORE_GFX_NO_GPU_INDIRECT", old);
  return result;
}
}
TEST_CASE(
    "DrawDispatch-1 nonindexed firstInstance survives every draw rung",
    "[DrawDispatch][first-instance]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool cpu = GENERATE(false, true);
  const bool control = qEnvironmentVariableIsSet("DRAW_DISPATCH_ABI_CONTROL");
  const auto r = firstInstance(api, cpu, control);
  if(r.skipped) SKIP(r.why);
  CAPTURE(backend_name(api), cpu, control, r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.image.valid());
  for(int i = 0; i < 8; ++i)
  {
    const auto px = r.image.at(4*i+2,32);
    std::fprintf(stderr, "DrawDispatch-1 api=%s cpu=%d control=%d strip=%d rgba=%u,%u,%u,%u\n",
                 backend_name(api), cpu, control, i, px[0], px[1], px[2], px[3]);
    CAPTURE(i);
    // Position and G are vertex-buffer identity; do not assume the backend's
    // gl_InstanceIndex includes baseInstance (Qt exposes a separate feature).
    if(i == 3 || i == 4) { CHECK(px[0] >= 253); CHECK(std::abs(int(px[1])-i) <= 2); }
    else CHECK(px[0] < 200);
  }
}
TEST_CASE("DrawDispatch layered indirect dispatch preserves volume slices", "[DrawDispatch][layered]")
{
  const auto api = GENERATE(from_range(platform_backends()));
  const bool cpu = GENERATE(false, true);
  const auto old = qgetenv("SCORE_GFX_NO_GPU_DISPATCH_INDIRECT");
  if(cpu) qputenv("SCORE_GFX_NO_GPU_DISPATCH_INDIRECT", "1");
  else qunsetenv("SCORE_GFX_NO_GPU_DISPATCH_INDIRECT");
  IsfResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    r = render_isf_chain(api, {artifact("DrawDispatch_layered.cs"), corpus("3d-slice-viewer.fs")}, {64,64}, 3);
  });
  if(old.isNull()) qunsetenv("SCORE_GFX_NO_GPU_DISPATCH_INDIRECT"); else qputenv("SCORE_GFX_NO_GPU_DISPATCH_INDIRECT", old);
  if(r.skipped) SKIP(r.skip_reason);
  CAPTURE(backend_name(api), cpu, r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  REQUIRE(r.outputs[0].valid());
  const auto px = r.outputs[0].center();
  std::fprintf(stderr, "DrawDispatch-layered api=%s cpu=%d rgba=%u,%u,%u,%u\n",
               backend_name(api), cpu, px[0], px[1], px[2], px[3]);
  CHECK(near(px, {64,128,192,255}, 2));
}
