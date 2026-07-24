#pragma once
#include "TransformHelper.hpp"

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <Threedim/TinyObj.hpp>
#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <cstdint>
#include <memory>

class QRhiResourceUpdateBatch;

namespace score::gfx
{
class RenderList;
struct Edge;
}

namespace Threedim
{

// Rasterize text into 3D geometry. Each glyph is converted to a
// QPainterPath, flattened into polygons, and tessellated via simple
// ear-clipping. Output is a scene_spec containing one scene_node with
// one mesh_component whose vertices describe the text in the XY plane
// (normal = +Z) around the origin.
//
// Limitations (v1):
//   - Holes are NOT handled. Glyphs with interior holes ("O", "D", "o",
//     "P" counter, etc.) render as solid shapes. Fix planned by adding
//     earcut.hpp or hole-bridging to the tessellator.
//   - Extrusion = 0 (flat). A later revision will extrude along -Z
//     with properly-oriented side walls.
//   - Tangents are synthesized as (1, 0, 0, 1) by ScenePreprocessor's
//     fallback — no per-vertex tangent computed here.
//
// Designed for VJ / title-card use rather than typography; single-line
// inputs only. For paragraph text, use TextToTexture on a quad.
class TextToMesh
{
public:
  halp_meta(name, "Text to Mesh")
  halp_meta(category, "Visuals/3D/Text")
  halp_meta(c_name, "text_to_mesh")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/text-to-mesh.html")
  halp_meta(uuid, "c8f2a4d5-6e9b-4d3a-b7f1-5c4e2d8a9f6b")

  struct ins
  {
    // Port-driven rebuild: controls trigger TextToMesh::rebuild() via
    // their update() callbacks; operator()() just republishes m_state.
    struct : halp::lineedit<"Text", "Hello">
    { void update(TextToMesh& n) { n.rebuild(); } } text;
    struct : halp::lineedit<"Font family", "Sans">
    { void update(TextToMesh& n) { n.rebuild(); } } font_family;
    struct : halp::spinbox_i32<"Font size", halp::irange{4, 512, 72}>
    { void update(TextToMesh& n) { n.rebuild(); } } font_size;
    struct : halp::toggle<"Bold">
    { void update(TextToMesh& n) { n.rebuild(); } } bold;
    struct : halp::toggle<"Italic">
    { void update(TextToMesh& n) { n.rebuild(); } } italic;
    // World-space height of a capital 'H'. Glyph paths come out in
    // pixel units from Qt; we scale them to this target so the mesh
    // lives at a sensible world scale regardless of font_size.
    struct : halp::hslider_f32<"Height", halp::range{0.01, 100., 1.}>
    { void update(TextToMesh& n) { n.rebuild(); } } height;
    // Centers the text around the origin on the X axis (vs. left-align
    // at X=0). Useful for title cards.
    struct : halp::toggle<"Center X">
    { void update(TextToMesh& n) { n.rebuild(); } } center_x;

    struct : PositionControl
    { void update(TextToMesh& n) { n.rebuild(); } } position;
    struct : RotationControl
    { void update(TextToMesh& n) { n.rebuild(); } } rotation;
    struct : ScaleControl
    { void update(TextToMesh& n) { n.rebuild(); } } scale;
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

  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  std::shared_ptr<ossia::scene_state> m_wrapped_state;
  CachedTRS m_cachedTRS{};
  // Mesh-rebuild cache — expensive tessellation only re-runs when text
  // or font parameters actually change.
  std::string m_cached_text;
  std::string m_cached_family;
  int m_cached_size{-1};
  bool m_cached_bold{false};
  bool m_cached_italic{false};
  float m_cached_height{-1.f};
  bool m_cached_center{false};
  std::shared_ptr<ossia::mesh_component> m_cached_mesh;
  int64_t m_version_counter{0};
  uint8_t m_pending_dirty{0xFF};

  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;
  ossia::gpu_slot_ref m_xform_ref{};
};

}
