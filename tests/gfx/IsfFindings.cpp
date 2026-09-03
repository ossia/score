// =============================================================================
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

// -----------------------------------------------------------------------------
// isf-multipass-storage-rw: pass 1 emits vec4(fract(TIME*.5), uv.y*.4, uv.x*.4,1)
// — the green/blue uv terms are non-zero regardless of TIME, so the output must
// NOT be uniformly black.
// -----------------------------------------------------------------------------
TEST_CASE(
    "FINDING isf-multipass-storage-rw final pass renders black (fixed: the uv\n     gradient is present on OpenGL and Vulkan, and is asserted here)",
    "[gfx][l3][isf][multipass][finding]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  const IsfResult r = render(backend, {corpus("isf-multipass-storage-rw.fs")});
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 1);
  const auto& img = r.outputs[0];
  REQUIRE(img.valid());

  const auto c = img.center();
  INFO("centre = (" << (int)c[0] << "," << (int)c[1] << "," << (int)c[2] << ")");
  // Correct behaviour: uv.y*0.4 / uv.x*0.4 make a visible gradient.
  CHECK(non_degenerate(img)); // was all-black; now the gradient is there
}

// -----------------------------------------------------------------------------
// isf-multipass-persistent-ssbo: pass 1 emits vec4(counter%256/255, uv.y*.3,
// uv.x*.3, 1) and the counter advances once per frame. Both the uv pattern and
// the per-frame red ramp must be visible.
// -----------------------------------------------------------------------------
TEST_CASE(
    "FINDING isf-multipass-persistent-ssbo final pass renders black (fixed: the\n     uv pattern and the per-frame red ramp are both present and asserted)",
    "[gfx][l3][isf][multipass][persistent][finding]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  struct Out
  {
    bool skipped = false;
    std::string skip_reason, backend, error;
    ReadbackImage f2, f8;
  } out;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto r2 = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-multipass-persistent-ssbo.fs")}, {64, 64}, 2);
    auto r8 = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-multipass-persistent-ssbo.fs")}, {64, 64}, 8);
    out.skipped = r2.skipped;
    out.skip_reason = r2.skip_reason;
    out.backend = r2.backend;
    out.error = r2.error.empty() ? r8.error : r2.error;
    if(!r2.outputs.empty())
      out.f2 = r2.outputs[0];
    if(!r8.outputs.empty())
      out.f8 = r8.outputs[0];
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.f2.valid());
  REQUIRE(out.f8.valid());

  INFO("f8 centre red=" << (int)out.f8.center()[0]);
  CHECK(non_degenerate(out.f8)); // was all-black
  // Correct behaviour: the counter ramp advances the red channel with frames.
  CHECK(int(out.f8.center()[0]) > int(out.f2.center()[0])); // was 0 == 0
}
