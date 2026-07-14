#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace Threedim
{

// Extracts a subtree from an incoming scene and emits it as a fresh
// scene_spec. The Solaris "Extract" pattern: if SceneGraphFilter is
// Prune (keeps the tree shape, drops non-matches), SceneSelector is
// Extract (gathers the matches and forgets the ancestors).
//
// Use case: pull out the camera, a light rig, or a character subtree
// so it can be re-transformed / re-materialized and then merged back
// in via SceneGroup.
//
// Rebase modes:
//   Preserve       : emit the subtree root as-is, so its transform
//                    remains in its original parent frame (the
//                    ancestors are gone but the transform still
//                    matches where it was).
//   ZeroOut        : drop the subtree's own TRS so it renders at the
//                    world origin. Useful when you want to re-place
//                    the extracted subtree via an upstream
//                    Transform3D / SceneGroup.
class SceneSelector
{
public:
  halp_meta(name, "Scene Selector")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "scene_selector")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/scene-selector.html")
  halp_meta(uuid, "6c4d8b3f-5e2a-4d1f-9c7b-8a3e5f0d7b4c")

  enum Mode
  {
    ByPath,
    ByName,
    ByIndex       // index into the root list (0 = first root)
  };

  enum Rebase
  {
    Preserve,
    ZeroOut
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
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[3]{"By path", "By name", "By index"};
        int init{0};
      };
      void update(SceneSelector& n) { n.rebuild(); }
    } mode;

    struct : halp::lineedit<"Path / Name", "">
    { void update(SceneSelector& n) { n.rebuild(); } } path;
    struct : halp::spinbox_i32<"Index", halp::irange{0, 1024, 0}>
    { void update(SceneSelector& n) { n.rebuild(); } } index;

    struct : halp::combobox_t<"Rebase", Rebase>
    {
      struct range
      {
        std::string_view values[2]{"Preserve transform", "Zero out"};
        int init{0};
      };
      void update(SceneSelector& n) { n.rebuild(); }
    } rebase;
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
  int m_cached_mode{-1};
  int m_cached_rebase{-1};
  int m_cached_index{-1};
  std::string m_cached_path;
  int64_t m_version_counter{0};
};

}
