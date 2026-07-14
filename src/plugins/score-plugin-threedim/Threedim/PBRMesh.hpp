#pragma once
#include "TransformHelper.hpp"

#include <ossia/dataflow/geometry_port.hpp>

#include <Gfx/Graph/GpuResourceRegistry.hpp>

#include <Threedim/TinyObj.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

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

// Wraps a GPU-resident geometry (the output of a compute-shader framework
// node — `halp::dynamic_gpu_geometry`) as a one-node scene_spec with a
// PBR material attached. The bridge between "CSF produces a geometry"
// and "scene-graph pipeline consumes scene_spec".
//
// Typical wiring:
//   CSFNode(mesh_out) → PBRMesh(mesh_in, texture_in) → ScenePreprocessor
//
// The node emits a single scene_node at the root holding:
//   - a scene_transform built from the TRS controls
//   - a mesh_component wrapping the GPU geometry into one mesh_primitive
//   - a direct material_component_ptr (also published into the scene's
//     here: one material_component carrying the factor controls + any
//     wired-in runtime textures)
//
// Texture inputs route through the Dynamic Texture pathway in
// ScenePreprocessor: non-null handles become `*Dyn<slot>` auxiliary-texture
// bindings that classic_pbr_full samples directly, no CPU upload, no
// array-layer copy. Unwired inputs fall through to the scalar factors.
class PBRMesh
{
public:
  halp_meta(name, "PBR Mesh")
  halp_meta(category, "Visuals/3D/Scene")
  halp_meta(c_name, "pbr_mesh")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url, "https://ossia.io/score-docs/processes/pbr-mesh.html")
  halp_meta(uuid, "d7a2f5c9-3e8b-4b1d-a6f2-5c8e9d1f3b7a")

  struct ins
  {
    struct
    {
      halp_meta(name, "Mesh");
      halp::dynamic_gpu_geometry mesh;
      float transform[16]{};
      bool dirty_mesh = false;
      bool dirty_transform = false;
    } geometry_in;

    // Texture slots. Non-null handle → emitted as a dynamic texture
    // on the material; null → shader falls back to the scalar factor.
    halp::gpu_texture_input<"Base Color Tex"> base_color_tex;
    halp::gpu_texture_input<"Metal Rough Tex"> metal_rough_tex;
    halp::gpu_texture_input<"Normal Tex"> normal_tex;
    halp::gpu_texture_input<"Emissive Tex"> emissive_tex;

    // PBR factors — used as-is by the material (no per-factor toggle:
    // defaults here match glTF defaults, so "untouched" controls produce
    // a reasonable neutral material).
    halp::hslider_f32<"Color R", halp::range{0., 1., 1.}> base_r;
    halp::hslider_f32<"Color G", halp::range{0., 1., 1.}> base_g;
    halp::hslider_f32<"Color B", halp::range{0., 1., 1.}> base_b;
    halp::hslider_f32<"Color A", halp::range{0., 1., 1.}> base_a;
    halp::hslider_f32<"Metallic", halp::range{0., 1., 0.}> metallic;
    halp::hslider_f32<"Roughness", halp::range{0., 1., 0.5}> roughness;
    halp::hslider_f32<"Emissive R", halp::range{0., 10., 0.}> em_r;
    halp::hslider_f32<"Emissive G", halp::range{0., 10., 0.}> em_g;
    halp::hslider_f32<"Emissive B", halp::range{0., 10., 0.}> em_b;
    halp::hslider_f32<"Emissive strength", halp::range{0., 10., 1.}> em_strength;

    // Root-node placement. Same TRS controls as Transform3D / Instancer
    // so the node stands alone without a separate transform upstream.
    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
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

  // Render-thread hooks. init allocates a Material arena slot and seeds
  // it with default bytes; update packs the factor fields from the
  // control inputs into a MaterialGPU and uploads to the slot; release
  // returns the slot. Texture references (textureRefs[]) are left at
  // tex_ref_none() here — the preprocessor resolves those during its
  // material-channel upload pass because only it knows the per-channel
  // dynamic-slot / static-layer assignments for the upstream handles.
  void init(score::gfx::RenderList& r, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& r, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);

  // Republished stable shared_ptr when nothing changed, so ScenePreprocessor's
  // identity/fingerprint caches stay warm.
  std::shared_ptr<const ossia::scene_state> m_wrapped_state;
  CachedTRS m_cachedTRS{};

  // Identity cache: upstream mesh-buffer handles + vertex/index count +
  // texture handles + factor values. Dirty if any change.
  void* m_cached_buf0{};
  int64_t m_cached_vertices{-1};
  int64_t m_cached_indices{-1};
  void* m_cached_tex[4]{};
  float m_cached_factors[10]{};
  int64_t m_version_counter{0};

  // Stable ids minted once on first rebuild and reused across every
  // subsequent rebuild so downstream fingerprint / SER / BVH caches stay
  // identity-stable.
  uint64_t m_material_stable_id{};
  uint64_t m_primitive_stable_id{};
  uint64_t m_xform_stable_id{};

  // Slots: one in the Material arena, one in RawTransform for the
  // emitted scene_transform. Allocated in init(), written in update(),
  // freed in release().
  score::gfx::GpuResourceRegistry::Slot material_slot;
  score::gfx::GpuResourceRegistry::Slot raw_transform_slot;

  // Ossia-facing snapshots. Written once in init() on the render
  // thread; copied onto the emitted material_component /
  // scene_transform raw_slot in operator()().
  ossia::gpu_slot_ref m_material_ref{};
  ossia::gpu_slot_ref m_xform_ref{};
};

}
