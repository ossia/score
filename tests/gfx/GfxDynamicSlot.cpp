// =============================================================================
// L3/registry — dynamic-texture-slot staleness sweep prevents a dangling bind
// (finding R2-#3, commit de81090de).
//
// SCOPE / HONESTY: the full end-to-end path (a video / NDI / window-capture
// producer that changes resolution mid-stream, feeding a scene material through
// ScenePreprocessorNode) is NOT drivable by the L3 render fixture — the only
// external touchpoint of the dynamic-slot registry is ScenePreprocessorNode,
// which needs a whole flattened scene + a resolution-changing GPU-texture
// producer the fixture cannot build. So this guards the fix at the UNIT the fix
// lives in: GpuResourceRegistry's dynamic-slot map.
//
// MECHANISM. resolveDynamicSlot() caches a NON-OWNING raw QRhiTexture* per slot,
// keyed by globalResourceId. When a producer swaps its texture (new id) the old
// slot keeps a dangling pointer that the consumer binds every frame (the 4-slot
// map rarely fills, so LRU eviction never fires) -> use-after-free. The fix
// stamps each slot resolved this frame and sweepStaleDynamicTextureSlots()
// (invoked per-frame from sweepMeshSlabs, after the resolve pass, before the bind
// pass) clears any slot not re-resolved since the previous sweep.
//
// This test drives ONLY public methods present in BOTH the pre- and post-fix
// engine (resolveDynamicSlot / sweepMeshSlabs / textureChannel), so it builds
// against either and observes the BEHAVIOUR: a slot whose texture is no longer
// resolved must be cleared (nulled) so it can never be bound as a dangling
// pointer.
//
// REGRESSION GUARD. After resolving texture A, then (a new frame) resolving only
// texture B and running the per-frame sweep, A's slot must be nullptr. Pre-fix
// (no sweep in sweepMeshSlabs) it stays pointing at the orphaned A. Backend-
// independent, but run on each available backend so a real QRhi exists.
//
//   DISPLAY=:0 SCORE_TEST_API=opengl ctest -R gfx_dynamic_slot
//   DISPLAY=:0 SCORE_TEST_API=vulkan ctest -R gfx_dynamic_slot
// =============================================================================

#include <score_test/Gfx.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using namespace score::test::gfx;
using Reg = score::gfx::GpuResourceRegistry;

namespace
{
struct Outcome
{
  bool skipped = false;
  std::string backend;
  bool ran = false;
  // Slot-vector snapshot after the second sweep.
  int slotCount = 0;
  bool slot0Cleared = false; // A's slot must be nulled by the fix
  bool bStillBound = false;  // B's slot must remain valid
};
}

TEST_CASE(
    "dynamic-slot registry clears a stale (orphaned) texture slot",
    "[gfx][l3][registry][dynamic-slot]")
{
  const auto backend = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(backend));

  Outcome out;
  out.backend = backend_name(backend);

  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    std::string probed;
    if(!probe_api(backend, probed))
    {
      out.skipped = true;
      return;
    }

    auto st = score::gfx::createRenderState(backend, QSize{32, 32}, nullptr);
    if(!st || !st->rhi)
    {
      out.skipped = true;
      return;
    }
    QRhi& rhi = *st->rhi;

    // Two DISTINCT textures => distinct globalResourceIds. A models the
    // producer's first texture; B the one it recreates after a resolution
    // change. Both are kept alive (the guard is about the SLOT being unbound,
    // not about A being freed) so nothing is ever dereferenced stale. Owned
    // here; declared outside the reg block so they outlive the assertions.
    auto* A = rhi.newTexture(QRhiTexture::RGBA8, QSize{16, 16}, 1);
    auto* B = rhi.newTexture(QRhiTexture::RGBA8, QSize{24, 24}, 1);
    A->create();
    B->create();

    {
      Reg reg;
      const auto ch = Reg::TextureChannel::BaseColor;

      // Frame 1: the material resolves A -> slot for A is stamped this frame.
      const int slotA = reg.resolveDynamicSlot(ch, A);
      reg.sweepMeshSlabs(1); // post-fix: sweep runs; A was just resolved -> kept

      // Frame 2: the producer swapped its texture; the material now resolves B
      // only. A is NOT re-resolved -> it is orphaned.
      const int slotB = reg.resolveDynamicSlot(ch, B);
      reg.sweepMeshSlabs(2); // post-fix: sweep clears A's slot; keeps B

      auto& state = reg.textureChannel(ch);
      out.slotCount = int(state.dynamicTextures.size());
      if(slotA >= 0 && slotA < out.slotCount)
        out.slot0Cleared = (state.dynamicTextures[slotA] == nullptr);
      // B must still be bound at its slot.
      if(slotB >= 0 && slotB < out.slotCount)
        out.bStillBound = (state.dynamicTextures[slotB] == B);
      out.ran = true;

      // reg destroyed at end of this block (before we free A/B): its dynamic
      // slots are non-owning, so it won't touch A/B.
    }

    delete A;
    delete B;
    st->destroy();
  });

  if(out.skipped)
    SKIP(out.backend + ": backend unavailable headless");
  INFO("backend=" << out.backend << " slots=" << out.slotCount);
  REQUIRE(out.ran);

  // THE GUARD: A's slot (not re-resolved in frame 2) must be cleared so it can
  // never be bound as a dangling pointer. Pre-fix (no sweep) it stays pointing
  // at the orphaned A.
  CHECK(out.slot0Cleared);
  // The live texture B must remain bound.
  CHECK(out.bStillBound);
}
