#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace Threedim
{

// Scene-graph-level duplicator. Given a prototype scene_spec, emits N
// cloned root nodes placed by a procedural pattern. Complementary to
// Instancer:
//
//   Instancer          = GPU-primitive instancing (one mesh, N instances,
//                        one draw call — scales to 1M+ particles, but the
//                        prototype is a single mesh).
//   SceneDuplicator    = scene-graph instancing (rich prototype w/
//                        hierarchy / multiple meshes / lights, N CPU
//                        clones each with its own TRS — scales to
//                        dozens-to-a-few-hundreds).
//
// Materials / animations / skeletons / environment pass through from the
// prototype unchanged (shared across clones). Only the root-level node
// tree is cloned so downstream path-based tooling addresses each clone
// independently via `/<prototype_root>_<n>/...`.
//
// Patterns:
//   Grid   — `count` clones laid out on an XZ grid with `spacing`. Y=0.
//   Ring   — `count` clones on a circle in the XZ plane of `radius`
//            centered at the origin, facing outward (rotated around Y).
//   Line   — `count` clones along +X with `spacing` separation.
//
// Downstream addressing: each clone's root node is named
// `<proto_name>_<idx>` (0-indexed), so:
//   SceneDuplicator(prototype=ChairScene, mode=Ring, count=8) →
//     /Chair_0, /Chair_1, … /Chair_7
//   ConfigurePrimitive(paths=["/Chair_*"], active=false) → disables all
class SceneDuplicator
{
public:
  halp_meta(name, "Scene Duplicator")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "scene_duplicator")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/scene-duplicator.html")
  halp_meta(uuid, "9e7a4b3d-5f2c-4a8b-9d1e-6c3f8b5d2a7e")

  enum Pattern
  {
    Grid,
    Ring,
    Line
  };

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
    struct : halp::combobox_t<"Pattern", Pattern>
    {
      struct range
      {
        std::string_view values[3]{"Grid", "Ring", "Line"};
        int init{0};
      };
      void update(SceneDuplicator& n) { n.rebuild(); }
    } pattern;

    struct : halp::spinbox_i32<"Count", halp::irange{1, 4096, 4}>
    { void update(SceneDuplicator& n) { n.rebuild(); } } count;
    struct : halp::hslider_f32<"Spacing", halp::range{0.01, 1000., 2.}>
    { void update(SceneDuplicator& n) { n.rebuild(); } } spacing;
    struct : halp::hslider_f32<"Radius", halp::range{0.01, 1000., 5.}>
    { void update(SceneDuplicator& n) { n.rebuild(); } } radius;
    // Grid mode: grid is `cols × rows` with cols ≈ round(sqrt(count)).
    // Exposed as a control so the user can force a specific aspect.
    // 0 = auto (square-ish).
    struct : halp::spinbox_i32<"Grid cols", halp::irange{0, 256, 0}>
    { void update(SceneDuplicator& n) { n.rebuild(); } } grid_cols;
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

  // Stable shared_ptr cached while inputs are unchanged — keeps
  // ScenePreprocessor's fingerprint fast-path warm.
  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  int m_cached_pattern{-1};
  int m_cached_count{-1};
  int m_cached_grid_cols{-1};
  float m_cached_spacing{-1.f};
  float m_cached_radius{-1.f};
  int64_t m_version_counter{0};
};

}
