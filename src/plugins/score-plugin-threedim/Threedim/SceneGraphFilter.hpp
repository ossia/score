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

// Scene-graph filter. Takes a scene in, emits a scene out whose node
// tree has been culled/pruned by a predicate selected via `mode`. The
// dropped nodes and their descendants are excluded from flattening
// downstream.
//
// Predicates run against each scene_node during the walk. Subtrees
// whose nodes all match are returned by shared_ptr identity (no
// cloning) so downstream caches stay warm on untouched branches.
//
// Path syntax: slash-joined scene_node::name chain from roots, glob
// wildcards (`*` matches anything except `/`, `**` matches across
// slashes). Example: `/*/Wheels/**` includes everything under any
// root whose first-level child is named "Wheels".
class SceneGraphFilter
{
public:
  halp_meta(name, "Scene Graph Filter")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "scene_graph_filter")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/scene-graph-filter.html")
  halp_meta(uuid, "3c7e9a5d-2f4b-4e6c-8b1a-0d5f7e3a9c8b")

  enum Mode
  {
    PassThrough,
    VisibleOnly,
    ByPath,
    ByName,
    ByComponent,
    ByMaterialTag,
    // SetVisibility mode: matching nodes have their `visible` flag
    // flipped to !Invert (Invert=false → hidden, Invert=true → shown).
    // Non-matching nodes kept untouched. Unlike the filter modes above
    // this DOESN'T drop nodes — they stay in the tree so downstream
    // material / transform / light data is preserved, just
    // render-invisible.
    SetVisibility,

    // Schema-field predicates. Operate on well-known
    // material_component / scene_node fields — no string hashing,
    // no glob. Each mode reads one field and compares against the
    // inline control.
    ByAlphaMode,        // material.alpha == (selected enum)
    ByShadowCaster,     // material.shadow_caster == (selected bool)
    ByReflectionCaster, // material.reflection_caster == (selected bool)
    ByPurpose,          // scene_node.purpose == (selected enum)

    // Property-dict predicates. Read scene_node::properties or
    // material_component::properties by key and compare against a
    // literal. Value type is inferred from the control (string/float/
    // int). Useful for user-authored metadata — USD extra attributes,
    // glTF material.extras JSON, custom layer tags.
    ByNodeProperty,     // scene_node.properties[key] matches value
    ByMaterialProperty  // material.properties[key] matches value
  };

  enum Component
  {
    Mesh,
    Light,
    Camera,
    Instance,
    Skeleton
  };

  enum AlphaMode
  {
    AlphaOpaque = 0,
    AlphaMask   = 1,
    AlphaBlend  = 2
  };

  enum Purpose
  {
    PurposeDefault = 0,
    PurposeRender  = 1,
    PurposeProxy   = 2,
    PurposeGuide   = 3
  };

  // Operator for property matches — extends beyond string-glob to
  // support numeric thresholds without a full predicate-DSL rollout.
  enum PropertyOp
  {
    PropEqual,
    PropNotEqual,
    PropLessThan,
    PropGreaterThan,
    PropContains  // substring match when value is string
  };

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    // Port-driven rebuild: controls trigger rebuild() via update();
    // upstream scene_in changes detected in operator()().
    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[13]{
            "Pass through", "Visible only", "By path", "By name",
            "By component", "By material tag", "Set visibility",
            "By alpha mode", "By shadow caster", "By reflection caster",
            "By purpose", "By node property", "By material property"};
        int init{0};
      };
      void update(SceneGraphFilter& n) { n.rebuild(); }
    } mode;

    // When true: drop nodes that match the predicate (the list acts
    // as an exclude filter). When false (default): keep matching
    // nodes, drop the rest.
    struct : halp::toggle<"Invert">
    { void update(SceneGraphFilter& n) { n.rebuild(); } } invert;

    // List inlets — user edits inline in the inspector. A halp
    // `val_port<vector<string>>` renders an editable N-row widget.
    // Each mode uses the relevant list; others are ignored.
    struct : halp::val_port<"Paths", std::vector<std::string>>
    { void update(SceneGraphFilter& n) { n.rebuild(); } } paths;
    struct : halp::val_port<"Names", std::vector<std::string>>
    { void update(SceneGraphFilter& n) { n.rebuild(); } } names;
    struct : halp::val_port<"Material tags", std::vector<std::string>>
    { void update(SceneGraphFilter& n) { n.rebuild(); } } material_tags;

    struct : halp::combobox_t<"Component", Component>
    {
      struct range
      {
        std::string_view values[5]{
            "Mesh", "Light", "Camera", "Instance", "Skeleton"};
        int init{0};
      };
      void update(SceneGraphFilter& n) { n.rebuild(); }
    } component;

    // Schema-field selectors. Unused in most modes; each dropdown is
    // read only by its corresponding Mode.
    struct : halp::combobox_t<"Alpha mode", AlphaMode>
    {
      struct range
      { std::string_view values[3]{"Opaque", "Mask", "Blend"}; int init{0}; };
      void update(SceneGraphFilter& n) { n.rebuild(); }
    } alpha_mode;

    struct : halp::combobox_t<"Purpose", Purpose>
    {
      struct range
      {
        std::string_view values[4]{"Default", "Render", "Proxy", "Guide"};
        int init{0};
      };
      void update(SceneGraphFilter& n) { n.rebuild(); }
    } purpose;

    struct : halp::toggle<"Caster flag">
    { void update(SceneGraphFilter& n) { n.rebuild(); } } caster_flag;

    // Property-match inputs (ByNodeProperty / ByMaterialProperty).
    // Key + operator + literal; value parsed as float when numeric,
    // string otherwise. Missing keys never match (predicate false).
    struct : halp::val_port<"Property key", std::string>
    { void update(SceneGraphFilter& n) { n.rebuild(); } } prop_key;

    struct : halp::combobox_t<"Property op", PropertyOp>
    {
      struct range
      {
        std::string_view values[5]{
            "equal", "not equal", "less than", "greater than",
            "contains (string)"};
        int init{0};
      };
      void update(SceneGraphFilter& n) { n.rebuild(); }
    } prop_op;

    struct : halp::val_port<"Property value", std::string>
    { void update(SceneGraphFilter& n) { n.rebuild(); } } prop_value;
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

  // Cache the last emitted scene_state so unchanged inputs don't churn
  // downstream identity caches.
  std::shared_ptr<const ossia::scene_state> m_cached_out;
  uint8_t m_pending_dirty{0xFF};
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  int m_cached_mode{-1};
  bool m_cached_invert{false};
  int m_cached_component{-1};
  std::vector<std::string> m_cached_paths;
  std::vector<std::string> m_cached_names;
  std::vector<std::string> m_cached_material_tags;

  // Tier-1 extensions. Cached scalar inputs so rebuild() can check
  // change-state cheaply against the current run's snapshot.
  int m_cached_alpha_mode{-1};
  int m_cached_purpose{-1};
  bool m_cached_caster_flag{false};
  std::string m_cached_prop_key;
  int m_cached_prop_op{-1};
  std::string m_cached_prop_value;

  int64_t m_version_counter{0};
};

}
