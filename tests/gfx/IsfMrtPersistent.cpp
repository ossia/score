// =============================================================================
// L3 ISF feature surface — MRT + PERSISTENT-SSBO (isolated crash finding).
//
// isf-mrt-persistent-ssbo.fs combines TWO colour attachments (MRT) with a
// PERSISTENT read_write storage buffer that a fragment writes each frame; both
// attachments sample the same counter. Correct behaviour: BOTH outputs show the
// counter ramp and it advances once per frame.
//
// Observed on this box (NVIDIA):
//   * OpenGL — outA (attachment 0) shows the ramp; outB (attachment 1) is BLANK
//              (the storage binding never reaches the 2nd MRT attachment's SRB).
//   * Vulkan — the fragment's storage buffers are NOT declared in the MRT
//              pipeline layout (validation: [Set 0 Binding 3/4] STORAGE_BUFFER
//              used but not in the pipeline layout) and the draw SEGFAULTS.
//
// This is a real engine finding. It lives in its OWN ctest target so its Vulkan
// crash cannot abort the other ISF groups. The assertions below check the
// correct behaviour and are expected RED (OpenGL: outB blank; Vulkan: crash)
// until the MRT+persistent-fragment-SSBO binding is fixed. DO NOT weaken to green.
//
// Run: DISPLAY=:0 ctest -R gfx --output-on-failure
//   (SCORE_TEST_API=opengl runs only the OpenGL finding without the Vulkan crash)
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

TEST_CASE(
    "FINDING isf-mrt-persistent-ssbo second attachment / Vulkan binding",
    "[gfx][l3][isf][mrt][persistent][finding]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  // NOTE: on Vulkan the pipeline build for this shader triggers an invalid
  // descriptor and the draw crashes inside QRhi — this target is isolated so
  // that crash is contained. OpenGL runs first and its result is reported.
  const IsfResult r = render(backend, {corpus("isf-mrt-persistent-ssbo.fs")}, {64, 64}, 5);
  if(r.skipped)
    SKIP(r.backend + ": " + r.skip_reason);
  INFO("backend=" << r.backend);
  REQUIRE(r.error.empty());
  REQUIRE(r.outputs.size() == 2); // outA + outB ports
  REQUIRE(r.outputs[0].valid());
  REQUIRE(r.outputs[1].valid());

  const auto a = r.outputs[0].center();
  const auto b = r.outputs[1].center();
  INFO("outA centre = (" << (int)a[0] << "," << (int)a[1] << "," << (int)a[2] << ")");
  INFO("outB centre = (" << (int)b[0] << "," << (int)b[1] << "," << (int)b[2] << ")");

  const std::array<uint8_t, 4> black{0, 0, 0, 255};
  // outA carries the ramp (red) — attachment 0 works on OpenGL.
  CHECK(!near(a, black, 6));
  // Correct behaviour: outB must ALSO carry the counter (its blue channel).
  CHECK(!near(b, black, 6)); // RED on OpenGL: outB is blank
}
