#include "SceneInspector.hpp"

#include <fmt/format.h>

#include <algorithm>

namespace Threedim
{

namespace
{

struct ComponentFlags
{
  bool mesh{false};
  bool light{false};
  bool camera{false};
  bool skeleton{false};
  bool instance{false};
  bool transform{false};
  std::string material_tag;  // First mesh primitive's material tag (or empty)
  int vertex_count{0};       // Summed across all mesh primitives
  int triangle_count{0};

  // Space-separated compact tag string, e.g. "[mesh][trans][mat=3][v=1024 t=512]"
  std::string tags(bool show_components, bool show_stats) const
  {
    std::string out;
    if(show_components)
    {
      if(mesh)      out += "[mesh]";
      if(light)     out += "[light]";
      if(camera)    out += "[cam]";
      if(skeleton)  out += "[skel]";
      if(instance)  out += "[inst]";
      if(transform) out += "[trans]";
    }
    if(show_stats)
    {
      if(mesh && (vertex_count > 0 || triangle_count > 0))
        out += fmt::format("[v={} t={}]", vertex_count, triangle_count);
      if(mesh && !material_tag.empty())
        out += fmt::format("[mat={}]", material_tag);
    }
    return out;
  }
};

// Scan this node's DIRECT children for non-scene-node payloads and
// record which kinds appear. Meshes additionally contribute their
// vertex/triangle counts for the stats output.
ComponentFlags detectComponents(const ossia::scene_node& node) noexcept
{
  ComponentFlags f;
  if(!node.has_children())
    return f;
  for(const auto& payload : *node.children)
  {
    if(auto* m = ossia::get_if<ossia::mesh_component_ptr>(&payload))
    {
      if(*m)
      {
        f.mesh = true;
        for(const auto& prim : (*m)->primitives)
        {
          f.vertex_count += int(prim.vertex_count);
          // Source primitive count for this topology. index_count == 0
          // means non-indexed; fall back to vertex_count.
          const int ic = int(prim.index_count);
          const int n = (ic > 0 ? ic : int(prim.vertex_count));
          switch(prim.topology)
          {
            using T = ossia::primitive_topology;
            case T::points:
              f.triangle_count += n;
              break;
            case T::lines:
              f.triangle_count += n / 2;
              break;
            case T::line_strip:
              f.triangle_count += std::max(0, n - 1);
              break;
            case T::triangles:
              f.triangle_count += n / 3;
              break;
            case T::triangle_strip:
            case T::triangle_fan:
              f.triangle_count += std::max(0, n - 2);
              break;
            case T::patches:
            case T::meshlets:
              // Not a "primitive count" in the user sense; skip.
              break;
          }
          if(f.material_tag.empty() && prim.material)
            f.material_tag = prim.material->tag;
        }
      }
    }
    else if(ossia::get_if<ossia::light_component_ptr>(&payload))
      f.light = true;
    else if(ossia::get_if<ossia::camera_component_ptr>(&payload))
      f.camera = true;
    else if(ossia::get_if<ossia::skeleton_component_ptr>(&payload))
      f.skeleton = true;
    else if(ossia::get_if<ossia::instance_component_ptr>(&payload))
      f.instance = true;
    else if(ossia::get_if<ossia::scene_transform>(&payload))
      f.transform = true;
  }
  return f;
}

// ───── Walker ────────────────────────────────────────────────────────
//
// Accumulates rows + readable + running stats. Called recursively on
// the scene_node subtree; handles Paths/Names/Tree modes inline so the
// tree glyphs are emitted at the right place (only Tree mode uses
// indentation prefixes, Paths and Names emit flat rows).

struct State
{
  SceneInspector::Mode mode;
  bool show_components;
  bool show_stats;
  bool include_hidden;
  int max_depth; // -1 = unlimited

  std::vector<std::string>* rows;
  std::string* readable;

  // Running stats.
  int node_count{0};
  int mesh_count{0};
  int light_count{0};
  int camera_count{0};
  int total_vertices{0};
  int total_triangles{0};
};

// Emit a single row for `node` in the current mode. `path` is the
// canonical slash-path from the root; `tree_prefix` is the box-drawing
// indentation used only in Tree mode (e.g., "│  ├── ").
void emitRow(
    State& s, const ossia::scene_node& node, const std::string& path,
    const std::string& tree_prefix)
{
  auto comp = detectComponents(node);

  // Update running stats.
  s.node_count++;
  if(comp.mesh)
    s.mesh_count++;
  if(comp.light)
    s.light_count++;
  if(comp.camera)
    s.camera_count++;
  s.total_vertices += comp.vertex_count;
  s.total_triangles += comp.triangle_count;

  const std::string tag_suffix = comp.tags(s.show_components, s.show_stats);
  const char* hidden_suffix = (!node.visible) ? "[hidden]"
                              : (!node.active) ? "[inactive]"
                                               : "";

  std::string row;
  switch(s.mode)
  {
    case SceneInspector::Paths:
      row = path.empty() ? std::string("/") : path;
      if(!tag_suffix.empty())
      {
        row += ' ';
        row += tag_suffix;
      }
      if(*hidden_suffix)
      {
        row += ' ';
        row += hidden_suffix;
      }
      break;
    case SceneInspector::Names:
      row = node.name.empty() ? std::string{"(unnamed)"} : node.name;
      if(!tag_suffix.empty())
      {
        row += ' ';
        row += tag_suffix;
      }
      if(*hidden_suffix)
      {
        row += ' ';
        row += hidden_suffix;
      }
      break;
    case SceneInspector::Tree:
      row = tree_prefix;
      row += node.name.empty() ? std::string{"(unnamed)"} : node.name;
      if(!tag_suffix.empty())
      {
        row += ' ';
        row += tag_suffix;
      }
      if(*hidden_suffix)
      {
        row += ' ';
        row += hidden_suffix;
      }
      break;
    case SceneInspector::Summary:
      // Summary mode emits roots only at top level; leaves handled by
      // the outer walker. Skip here.
      return;
  }

  s.rows->push_back(row);
  *s.readable += row;
  *s.readable += '\n';
}

// Depth-first walk. `depth` is 0 at the root. `prefix_trunk` is the
// continuation prefix inherited from ancestors ("│  " for "still more
// siblings on that ancestor", "   " for "ancestor was last child").
// `is_last_child` is whether this node is its parent's last child —
// controls the ├── vs └── glyph.
void walk(
    State& s, const ossia::scene_node_ptr& node, const std::string& path,
    const std::string& prefix_trunk, bool is_last_child, int depth)
{
  if(!node)
    return;
  if(!s.include_hidden && (!node->active || !node->visible))
    return;
  if(s.max_depth >= 0 && depth > s.max_depth)
    return;

  // Tree-mode glyph for this node.
  std::string tree_prefix;
  if(s.mode == SceneInspector::Tree && depth > 0)
    tree_prefix = prefix_trunk + (is_last_child ? "└── " : "├── ");
  // depth == 0 (root) gets no glyph — it stands alone.

  emitRow(s, *node, path, tree_prefix);

  if(!node->has_children())
    return;

  // Collect the subset of children that are scene_node_ptrs (we only
  // recurse into those; scene_transform / component payloads have
  // already been folded into the parent's row via detectComponents).
  std::vector<const ossia::scene_node_ptr*> child_nodes;
  child_nodes.reserve(node->children->size());
  for(const auto& p : *node->children)
  {
    if(auto* sub = ossia::get_if<ossia::scene_node_ptr>(&p))
      if(*sub)
        child_nodes.push_back(sub);
  }

  const std::string next_trunk = (depth == 0)
      ? std::string{}
      : (prefix_trunk + (is_last_child ? "    " : "│   "));

  for(std::size_t i = 0; i < child_nodes.size(); ++i)
  {
    const bool last = (i + 1 == child_nodes.size());
    const auto& sub = *child_nodes[i];
    std::string childPath = path + '/' + sub->name;
    walk(s, sub, childPath, next_trunk, last, depth + 1);
  }
}

} // namespace

void SceneInspector::operator()()
{
  const auto& in = inputs.scene_in.scene;
  const ossia::scene_state* in_state = in.state.get();
  const int64_t in_version = in_state ? in_state->version : -1;

  const bool unchanged
      = m_cached_valid && m_cached_in_state == in_state
        && m_cached_in_version == in_version
        && m_cached_mode == inputs.mode.value
        && m_cached_show_components == inputs.show_components.value
        && m_cached_show_stats == inputs.show_stats.value
        && m_cached_include_hidden == inputs.include_hidden.value
        && m_cached_max_depth == inputs.max_depth.value;
  if(unchanged)
    return;

  m_cached_in_state = in_state;
  m_cached_in_version = in_version;
  m_cached_mode = inputs.mode.value;
  m_cached_show_components = inputs.show_components.value;
  m_cached_show_stats = inputs.show_stats.value;
  m_cached_include_hidden = inputs.include_hidden.value;
  m_cached_max_depth = inputs.max_depth.value;
  m_cached_valid = true;

  auto& rows = outputs.rows.value;
  auto& readable = outputs.readable.value;
  rows.clear();
  readable.clear();

  outputs.node_count.value = 0;
  outputs.mesh_count.value = 0;
  outputs.light_count.value = 0;
  outputs.camera_count.value = 0;
  outputs.material_count.value = 0;
  outputs.total_vertices.value = 0;
  outputs.total_triangles.value = 0;

  if(!in_state)
  {
    rows.push_back("(empty scene)");
    readable = "(empty scene)\n";
    return;
  }

  // Material count comes straight from the state.
  outputs.material_count.value
      = in_state->materials ? int(in_state->materials->size()) : 0;

  State s{
      Mode(inputs.mode.value),
      inputs.show_components.value,
      inputs.show_stats.value,
      inputs.include_hidden.value,
      inputs.max_depth.value,
      &rows,
      &readable,
      0, 0, 0, 0, 0, 0};

  if(inputs.mode.value == Summary)
  {
    // Summary: one block per root with aggregate stats, plus a global
    // materials section + active camera if set.
    if(in_state->roots)
    {
      fmt::format_to(
          std::back_inserter(readable), "Scene: {} root(s)\n",
          in_state->roots->size());
      for(const auto& r : *in_state->roots)
      {
        if(!r)
          continue;
        State local = s;
        local.rows = &rows;
        local.readable = &readable;
        walk(local, r, "/" + r->name, std::string{}, true, 0);
        s.node_count += local.node_count;
        s.mesh_count += local.mesh_count;
        s.light_count += local.light_count;
        s.camera_count += local.camera_count;
        s.total_vertices += local.total_vertices;
        s.total_triangles += local.total_triangles;
      }
    }
    std::string hdr = fmt::format(
        "== Scene Summary ==\n"
        "  nodes:     {}\n"
        "  meshes:    {}\n"
        "  lights:    {}\n"
        "  cameras:   {}\n"
        "  materials: {}\n"
        "  vertices:  {}\n"
        "  triangles: {}\n",
        s.node_count, s.mesh_count, s.light_count, s.camera_count,
        outputs.material_count.value, s.total_vertices, s.total_triangles);
    readable.insert(0, hdr);
    rows.insert(rows.begin(), std::move(hdr));
  }
  else
  {
    if(in_state->roots)
    {
      for(const auto& r : *in_state->roots)
      {
        if(!r)
          continue;
        const std::string rootPath = "/" + r->name;
        walk(s, r, rootPath, std::string{}, true, 0);
      }
    }
  }

  outputs.node_count.value = s.node_count;
  outputs.mesh_count.value = s.mesh_count;
  outputs.light_count.value = s.light_count;
  outputs.camera_count.value = s.camera_count;
  outputs.total_vertices.value = s.total_vertices;
  outputs.total_triangles.value = s.total_triangles;
}

} // namespace Threedim
