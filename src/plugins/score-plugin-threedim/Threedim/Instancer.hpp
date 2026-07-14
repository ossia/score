#pragma once
#include "TransformHelper.hpp"

#include <Threedim/TinyObj.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

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

// GPU-instancing authoring node. Takes a scene containing a mesh and a
// GPU buffer of per-instance transforms (+ optional colors / custom),
// emits a scene_spec wrapping an `instance_component` that
// ScenePreprocessor forwards to downstream shaders as the standard
// `instance_transforms` / `instance_colors` / `instance_custom`
// auxiliary buffers.
//
// Consumer shaders (classic_pbr_mdi and friends) read the per-instance
// attributes via the existing VERTEX_INPUTS location 3..5 convention
// already in `GeometryToBufferStrategies.hpp`:
//   location 3 = per-instance translation / rotation / transform_matrix
//   location 4 = per-instance color0
//   location 5 = per-instance (scale / custom)
// No shader edits needed — the aux-buffer naming convention is the
// same one MeshInstancer uses.
//
// Transform formats (packed floats per instance):
//   mat4        : 16 floats (full 4×4 matrix, column-major)
//   trs         : 10 floats (3 translation + 4 quaternion + 3 scale)
//   translation : 3 floats  (position-only, rotation / scale = identity)
class Instancer
{
public:
  halp_meta(name, "Instancer")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "instancer")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/instancer.html")
  halp_meta(uuid, "5e8a2c7f-9b4d-4e3a-a1c6-2d7f0b3e8c4a")

  enum TransformFormat
  {
    Mat4,
    TRS,
    Translation
  };

  struct ins
  {
    struct
    {
      halp_meta(name, "Scene In");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_in;

    halp::gpu_buffer_input<"Transforms"> transforms;
    halp::gpu_buffer_input<"Colors"> colors;
    halp::gpu_buffer_input<"Custom"> custom;

    // Optional point-cloud geometry input. When wired, its semantic
    // attributes override the raw buffer inputs above:
    //   translation / position → Transforms buffer (Translation mode)
    //   transform_matrix       → Transforms buffer (Mat4 mode)
    //   color0                 → Colors buffer
    // The `count` inlet is overridden by the geometry's vertex_count
    // when this is wired (so downstream doesn't need to track the
    // point-cloud size manually). Lets shaderlib presets
    // (RandomScatter, EmitFromMesh, CurlNoiseForce, NoiseField etc.)
    // feed Instancer directly without a glue repack.
    struct
    {
      halp_meta(name, "Points");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } points;

    // Port-driven rebuild: scalar controls trigger Instancer::rebuild().
    // Upstream scene_in / buffer handles are detected in operator()()
    // because they can change without a port-update event.
    struct : halp::combobox_t<"Format", TransformFormat>
    {
      struct range
      {
        std::string_view values[3]{"mat4", "trs", "translation"};
        int init{0};
      };
      void update(Instancer& n) { n.rebuild(); }
    } format;

    struct : halp::spinbox_i32<"Count", halp::irange{1, 1000000, 1}>
    { void update(Instancer& n) { n.rebuild(); } } count;

    // Optional TRS applied to the prototype before instancing — lets
    // the node place the instanced cloud without a separate
    // Transform3D upstream.
    struct : PositionControl
    { void update(Instancer& n) { n.rebuild(); } } position;
    struct : RotationControl
    { void update(Instancer& n) { n.rebuild(); } } rotation;
    struct : ScaleControl
    { void update(Instancer& n) { n.rebuild(); } } scale;
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

  // Cache so we republish a stable shared_ptr when inputs haven't
  // changed — ScenePreprocessor's identity caches stay warm.
  std::shared_ptr<ossia::scene_state> m_wrapped_state;
  uint8_t m_pending_dirty{0xFF};
  CachedTRS m_cachedTRS{};
  // Track input identity to detect when a rebuild is needed without
  // relying on buffer-contents equality.
  const ossia::scene_state* m_cached_in_state{};
  void* m_cached_transforms{};
  void* m_cached_colors{};
  void* m_cached_custom{};
  int32_t m_cached_count{-1};
  int m_cached_format{-1};
  // For the point-cloud input: cache the primary-buffer identity so we
  // detect upstream handle replacements without poking every buffer
  // every frame.
  void* m_cached_points_buf{};
  int64_t m_cached_points_vertices{-1};
  int64_t m_version_counter{0};

  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;
  ossia::gpu_slot_ref m_xform_ref{};
};

}
