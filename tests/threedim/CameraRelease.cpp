// L3 regression guard — split/threedim finding #8 (Camera::release() must reset
// m_state).
//
// Every sibling transform producer (Light / Transform3D / CameraArray) resets
// its cached scene_state in release() so that after a RenderList release+init
// cycle the next operator()() rebuilds against freshly-allocated arena slots.
// Camera::release() omitted m_state.reset(), so operator()() (which only
// rebuilds when m_state is null) republished a stale scene_transform whose
// raw_slot still embedded the OLD, now-freed RawTransform index — aliasing
// another producer's world-transform slot and drifting each cycle. The fix adds
// m_state.reset() to Camera::release().
//
// Observable, GPU-free assertion: build m_state via operator()(), call
// release(), and assert m_state was reset AND the next operator()() rebuilds a
// FRESH state (new shared_ptr, bumped version). Pre-fix, release() leaves the
// old m_state in place (RED).
//
// release() takes a RenderList& but, with both arena slots at their default
// (invalid) value, provably never dereferences it — the only uses of `r` in
// Camera::release() are guarded by raw_*_slot.valid(). We therefore hand it a
// reference into inert storage it is contractually forbidden to touch.

#include <Threedim/Camera.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <new>

TEST_CASE(
    "Camera::release resets the cached scene_state", "[threedim][camera][f8]")
{
  Threedim::Camera cam;

  // First tick seeds m_state.
  cam();
  REQUIRE(cam.m_state != nullptr);
  const auto firstState = cam.m_state; // keep the object alive for pointer cmp
  const int64_t firstVersion = cam.m_state->version;

  // Both arena slots are invalid by default, so release() never touches its
  // RenderList& argument. Provide inert, correctly-aligned storage for the
  // reference; it must never be dereferenced by the callee.
  alignas(std::max_align_t) static unsigned char storage[64]{};
  auto& inertRenderList
      = *reinterpret_cast<score::gfx::RenderList*>(&storage[0]);

  REQUIRE_FALSE(cam.raw_camera_slot.valid());
  REQUIRE_FALSE(cam.raw_transform_slot.valid());

  cam.release(inertRenderList);

  // The fix: m_state is cleared so the next tick rebuilds from scratch.
  CHECK(cam.m_state == nullptr);

  // Next tick rebuilds a genuinely fresh state (new allocation + new version),
  // not a republished stale one.
  cam();
  REQUIRE(cam.m_state != nullptr);
  CHECK(cam.m_state != firstState);
  CHECK(cam.m_state->version > firstVersion);
}
