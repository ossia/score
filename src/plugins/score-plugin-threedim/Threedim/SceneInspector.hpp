#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Threedim
{

// Read-only introspection node for scene_spec. Walks the incoming
// scene tree and emits:
//   - `Rows`: a list of strings, one per node. In Paths mode each row
//     is a canonical slash-path (`/Root/Body/Wheels`) you can copy
//     directly into SceneGraphFilter(paths=...) / ConfigurePrimitive /
//     SceneSelector. In Tree mode each row is indented with
//     box-drawing glyphs for visual hierarchy. In Names mode each row
//     is a bare node name.
//   - `Readable`: a formatted multi-line dump of the same information,
//     suitable to pipe into Ui::TextBox for a wider-view inspector.
//   - Scalar stats: node / mesh / light / camera / material counts
//     plus totalled triangle and vertex counts.
//
// Bridges the "what paths exist in this scene?" question that was
// previously unanswerable from the user's side — filter/selector
// nodes need string patterns, and without a way to enumerate the
// tree the user has to guess. Drop this node between a loader and a
// filter, read the Rows list, paste the path you want into the
// downstream node.
class SceneInspector
{
public:
  halp_meta(name, "Scene Inspector")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "scene_inspector")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/scene-inspector.html")
  halp_meta(uuid, "b5f2c8a3-4d1e-4b7f-9e6c-3a8d5f0b2c9e")

  enum Mode
  {
    Paths,     // canonical slash-paths, directly copy-pasteable
    Names,     // bare node names (may have duplicates)
    Tree,      // indented with ├──/└── glyphs
    Summary    // high-level per-root summary + counts
  };

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    struct : halp::combobox_t<"Mode", Mode>
    {
      struct range
      {
        std::string_view values[4]{"Paths", "Names", "Tree", "Summary"};
        int init{0};
      };
    } mode;

    halp::toggle<"Show components"> show_components;
    halp::toggle<"Show stats"> show_stats;
    halp::toggle<"Include hidden"> include_hidden;
    halp::spinbox_i32<"Max depth", halp::irange{-1, 64, -1}> max_depth;
  } inputs;

  struct outs
  {
    halp::val_port<"Rows", std::vector<std::string>> rows;
    halp::val_port<"Readable", std::string> readable;

    halp::val_port<"Node count", int> node_count;
    halp::val_port<"Mesh count", int> mesh_count;
    halp::val_port<"Light count", int> light_count;
    halp::val_port<"Camera count", int> camera_count;
    halp::val_port<"Material count", int> material_count;
    halp::val_port<"Total triangles", int> total_triangles;
    halp::val_port<"Total vertices", int> total_vertices;
  } outputs;

  void operator()();

  // Identity + version cache: if inputs haven't changed we skip the
  // whole walk. Matches the pattern used by SceneGraphFilter etc.
  const ossia::scene_state* m_cached_in_state{};
  int64_t m_cached_in_version{-1};
  int m_cached_mode{-1};
  bool m_cached_show_components{false};
  bool m_cached_show_stats{false};
  bool m_cached_include_hidden{false};
  int m_cached_max_depth{-2};
  bool m_cached_valid{false};
};

}
