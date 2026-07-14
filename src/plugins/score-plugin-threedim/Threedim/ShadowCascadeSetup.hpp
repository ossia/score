#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>

namespace Threedim
{

// Authors a `shadow_cascades_info` for the scene from the active camera
// frustum and a directional-light direction. Consumed by:
//   - a depth-only shadow_cascades pass (one draw per cascade)
//   - classic_pbr_full's PCF sampling at final shading
//
// Practical-split strategy: blend uniform and logarithmic splits with a
// λ parameter (Engel / Tabellion). λ=0 → pure uniform (equal depth
// intervals, wastes near-plane resolution), λ=1 → pure log (near-plane
// heavy, far cascades get almost no area). λ≈0.5 is a good default for
// interactive scenes.
//
// Each cascade's light view_projection fits the camera frustum slice to
// a square orthographic light-space box centered at the slice's world-
// space center, oriented along the light direction.
class ShadowCascadeSetup
{
public:
  halp_meta(name, "Shadow Cascade Setup")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "shadow_cascade_setup")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/shadow-cascade-setup.html")
  halp_meta(uuid, "7f4d8c2a-9e5b-4f6a-a3d2-1e8c6b9d7f4a")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    // Port-driven rebuild: controls trigger rebuild(); upstream
    // scene_in changes detected in operator()().
    struct : halp::spinbox_i32<"Cascade count", halp::irange{1, 8, 4}>
    { void update(ShadowCascadeSetup& n) { n.rebuild(); } } cascade_count;
    struct : halp::hslider_f32<"Shadow distance", halp::range{1., 10000., 100.}>
    { void update(ShadowCascadeSetup& n) { n.rebuild(); } } shadow_distance;
    struct : halp::hslider_f32<"Split lambda", halp::range{0., 1., 0.5}>
    { void update(ShadowCascadeSetup& n) { n.rebuild(); } } lambda;
    // Manual near/far override for the camera (the scene_state doesn't
    // currently expose the active camera's near/far on an accessible
    // path — these let the user match them). Typical defaults work for
    // the Camera node's default near=0.1 / far=1000.
    struct : halp::hslider_f32<"Camera near", halp::range{0.001, 10., 0.1}>
    { void update(ShadowCascadeSetup& n) { n.rebuild(); } } camera_near;
    struct : halp::hslider_f32<"Camera far", halp::range{1., 100000., 1000.}>
    { void update(ShadowCascadeSetup& n) { n.rebuild(); } } camera_far;
    // Directional-light override. Normally inherited from the first
    // directional light in the scene, but some pipelines (e.g. a single
    // orbiting light without a Light node) benefit from setting this
    // directly.
    struct : halp::xyz_spinboxes_f32<"Light direction", halp::range{-1., 1., 0.}>
    { void update(ShadowCascadeSetup& n) { n.rebuild(); } } light_direction;
  } inputs;

  struct outs
  {
    struct
    {
      halp_meta(name, "Scene Out");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  void rebuild();
  void operator()();

  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  int m_cached_count{-1};
  float m_cached_distance{-1.f};
  float m_cached_lambda{-1.f};
  float m_cached_near{-1.f};
  float m_cached_far{-1.f};
  float m_cached_dir[3]{};
  int64_t m_version_counter{0};
};

}
