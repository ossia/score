#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Threedim
{

// Authors metadata flags on matching scene_nodes: active, visible.
// Matches Solaris's "Configure Primitive" LOP, trimmed to the flags
// that are meaningful for a live renderer. (USD also has `kind` and
// `purpose` fields; we can add those later if needed — for now they
// don't change rendering behaviour.)
//
// Usage pattern:
//   glTF → ConfigurePrimitive(paths=["*/chairs/*"], active=false) → ScenePreprocessor
// disables the entire `chairs` subtree non-destructively — flipping
// the toggle re-activates it without reloading the glTF or rebuilding
// any GPU state.
//
// `visible` acts at the leaf level (hides from rendering but keeps the
// subtree composed); `active` is stronger (skips the subtree in the
// flatten walk entirely — no transforms applied, no data uploaded).
class ConfigurePrimitive
{
public:
  halp_meta(name, "Configure Primitive")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "configure_primitive")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/configure-primitive.html")
  halp_meta(uuid, "4b8e9d2a-7c5f-4e3a-9b1c-3d2f5e8a7b9c")

  enum Mode
  {
    // Applies the flags to every matching node. Non-matching nodes
    // keep their existing flags (no change).
    SetActive,
    SetInactive,
    SetVisible,
    SetInvisible,
    // Apply both at once — useful for "this subtree is off right now".
    SetActiveAndVisible,
    SetInactiveAndInvisible
  };

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    // Port-driven rebuild: controls trigger rebuild() via update().
    // scene_in pointer/version changes detected in operator()().
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[6]{
            "Set active",    "Set inactive",
            "Set visible",   "Set invisible",
            "Active + visible", "Inactive + invisible"};
        int init{0};
      };
      void update(ConfigurePrimitive& n) { n.rebuild(); }
    } mode;

    // Path-glob list. Same syntax as SceneGraphFilter: `*` wildcards
    // within a segment, `**` crosses slashes, `?` single char, literal
    // names otherwise.
    struct : halp::val_port<"Paths", std::vector<std::string>>
    { void update(ConfigurePrimitive& n) { n.rebuild(); } } paths;
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
  std::vector<std::string> m_cached_paths;
  int64_t m_version_counter{0};
};

}
