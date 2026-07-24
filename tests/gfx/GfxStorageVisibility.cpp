// =============================================================================
// L3 — storage-visibility binding-drift guard (finding R2-#6, commit 8ab01f368).
//
// isf-storage-visibility-drift.fs declares a storage_input `pad` VISIBILITY:"all"
// BEFORE a PERSISTENT read_write storage `counter`. libisf's GLSL codegen only
// emits / advances its binding counter for the graphics-visibility set, so it
// SKIPS `pad`; the runtime SRB assignment must agree. The pre-fix runtime skipped
// only stages=={} (visibilityToStages("all") is non-empty -> fragment), so `pad`
// consumed a graphics binding the codegen never emitted, drifting `counter` (and
// its `_prev` twin) one slot. `counter`'s writes then landed on `pad`'s buffer and
// `counter_prev` read a buffer the shader never wrote — the persistent ping-pong
// accumulator is stuck. Fix: gate collectGraphicsStorageResources on the same
// is_graphics_visibility predicate as the codegen (skip "all"/unknown/compute).
//
// OBSERVABLE (mirrors IsfPersistent's short-vs-long comparison): the counter
// advances +1 per frame ONLY if the binding is correct. A short run (few frames)
// and a long run (more frames) are compared — with the fix the encoded grey level
// is higher for the long run; with the drift the accumulator is stuck and the two
// runs read back the same grey.
//
// REGRESSION GUARD. long-run grey MUST exceed short-run grey on both backends.
// GREEN on OpenGL and Vulkan. Do NOT weaken.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_storage_visibility
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_storage_visibility
// =============================================================================
#include "IsfTestCommon.hpp"

using namespace score::test::gfx;
using namespace score::test::gfx::isf;

TEST_CASE(
    "storage VISIBILITY:all does not drift a later persistent binding",
    "[gfx][l3][isf][binding][storage-visibility]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  struct Run
  {
    bool skipped = false;
    std::string skip_reason, backend, error;
    ReadbackImage shortRun, longRun;
  } out;

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    auto r3 = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-storage-visibility-drift.fs")}, {64, 64}, 3);
    auto r11 = score::test::gfx::render_isf_chain(
        backend, {corpus("isf-storage-visibility-drift.fs")}, {64, 64}, 11);
    out.skipped = r3.skipped;
    out.skip_reason = r3.skip_reason;
    out.backend = r3.backend;
    out.error = r3.error.empty() ? r11.error : r3.error;
    if(!r3.outputs.empty())
      out.shortRun = r3.outputs[0];
    if(!r11.outputs.empty())
      out.longRun = r11.outputs[0];
  });

  if(out.skipped)
    SKIP(out.backend + ": " + out.skip_reason);
  INFO("backend=" << out.backend);
  REQUIRE(out.error.empty());
  REQUIRE(out.shortRun.valid());
  REQUIRE(out.longRun.valid());

  // Grey encodes (counter mod 16) * 16. A correct binding advances the counter
  // +1 per frame, so 3 frames -> ~48 and 11 frames -> ~176 (both < 256, no wrap).
  const int gShort = out.shortRun.center()[0];
  const int gLong = out.longRun.center()[0];
  INFO("counter grey short(3)=" << gShort << " long(11)=" << gLong);

  // The counter is actually running (non-black) ...
  CHECK(gLong > 40);
  // ... and advances with frame count. With the binding drift the accumulator is
  // stuck (counter writes hit `pad`, counter_prev reads a never-written buffer),
  // so short and long read back the same stuck value.
  CHECK(gLong > gShort + 40);
}
