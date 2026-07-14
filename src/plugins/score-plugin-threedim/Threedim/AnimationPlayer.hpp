#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Threedim
{

// Samples an incoming scene's animation channels at a user-provided
// time and emits a scene_spec whose animated scene_nodes carry updated
// scene_transform payloads (TRS) or whose skeletons carry updated bone
// poses. Passthrough when the input scene has no animations.
//
// Sampling model:
//   - animation_channel.target_node_id refers to a scene_node::id.
//   - target_path ∈ {translation, rotation, scale, weights, custom}.
//   - `times` + `values` hold the keyframes; `interpolation` is step /
//     linear / cubic_spline.
//
// Output layout:
//   - For TRS channels: find the first `scene_transform` payload in
//     the matching node's children (the convention GltfParser /
//     FbxParser follow — they prepend one per node) and override its
//     translation/rotation/scale fields.
//   - Subtrees that don't touch any animated node are shared as-is
//     (shared_ptr reuse), so downstream identity caches stay hot
//     outside the animated branch.
//   - Materials / skeletons / cameras / environment pass through by
//     shared_ptr identity.
//
// Currently unsupported (passthrough):
//   - weights (morph targets).
//   - custom paths.
//   - skeletal joint tracks that target joints inside a
//     skeleton_component rather than scene_node ids.
// These are follow-ups; they need the same sample-and-override pattern
// but on different storage.
class AnimationPlayer
{
public:
  halp_meta(name, "Animation Player")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "animation_player")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/animation-player.html")
  halp_meta(uuid, "2b4d7e8c-3a5f-4b9d-91c6-8d2e0f3a7b5e")

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    halp::hslider_f32<"Time", halp::range{0., 3600., 0.}> time;
    halp::hslider_f32<"Speed", halp::range{-4., 4., 1.}> speed;
    halp::toggle<"Loop"> loop;
    // When unset, 0 = first animation_component, 1 = second, …. -1 =
    // blend all (sum of all channels — useful when animations target
    // disjoint node sets, which is common for glTF scenes). Clamped to
    // the number of components at sample time.
    halp::spinbox_i32<"Clip index", halp::irange{-1, 32, -1}> clip_index;
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

  void operator()();

  std::shared_ptr<const ossia::scene_state> m_cached_state;
  int64_t m_version_counter{0};

  // Previous time — used only for the "speed" control's time advance;
  // if the user is wiring a direct time inlet, this is ignored.
  float m_prev_time{0.f};
};

}
