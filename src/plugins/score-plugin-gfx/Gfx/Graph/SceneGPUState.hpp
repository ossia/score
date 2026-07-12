#pragma once
#include <ossia/dataflow/geometry_port.hpp>

#include <QMatrix4x4>

#include <cstdint>
#include <vector>

namespace score::gfx
{

// GPU-friendly structures for packing scene data into UBOs/SSBOs. All matrices
// are column-major.
//
// Raw* structs are written by source nodes (Camera, Light, Transform3D,
// EnvironmentLoader) into their own GpuResourceRegistry arena slot at their
// operator()() time: view-independent, no aspect-ratio math or scene-graph
// composition. The cooked structs (CameraUBOData, LightGPU, MaterialGPU,
// PerDrawGPU, WorldTransformMat4) are produced by ScenePreprocessor from those
// arenas plus render-target state, and are what consumer shaders bind.
//
// Materials and env are composition-independent, so Raw == Cooked for them and
// MaterialGPU / EnvParamsUBO are used directly from source nodes.

#pragma pack(push, 1)

// Consumer shaders read RawLightData directly from the RawLight arena
// and compose world-space direction via world_transforms[transform_slot].

// Scene-level UBO: camera + global scene data.
struct SceneUBO
{
  float view[16]{};
  float projection[16]{};
  float viewProjection[16]{};
  float cameraPosition[4]{}; // xyz = position, w = padding
  float time{};
  int32_t lightCount{};
  int32_t materialCount{};
  float padding0{};
  float ambientColor[4]{0.03f, 0.03f, 0.03f, 1.f};
};

// Per-mesh UBO: model transform for the current draw call.
struct MeshUBO
{
  float model[16]{};
  float modelViewProjection[16]{};
  float normalMatrix[12]{}; // mat3 in std140 = 3 × vec4 (48 bytes)
  int32_t materialIndex{};
  float padding[3]{};
};

// Packed 32-bit texture reference stored in MaterialGPU::textureRefs[]:
//   bits 31..30 : source (0 = NONE, 1 = STATIC pool, 2 = DYNAMIC pool)
//   bits 29..24 : bucket index within the selected pool
//   bits 23.. 0 : layer index within the bucket's texture array
//
// 0xFFFFFFFF is the "no texture" sentinel: the shader falls back to the
// constant baseColor factor, metallic_factor, and so on.
inline constexpr uint32_t tex_ref_none() { return 0xFFFFFFFFu; }
inline constexpr uint32_t tex_ref_static(uint32_t bucket, uint32_t layer)
{
  // Packed layout: source:2 | bucket:7 | layer:23. The 7-bit bucket field gives
  // headroom for 128 buckets against a runtime cap of kMaxBuckets = 16 in
  // GpuResourceRegistry.hpp; raising the cap only needs larger shader sampler
  // arrays, not a new encoding. Shader-side decode is `(ref >> 23) & 0x7Fu` for
  // the bucket and `ref & 0x007FFFFFu` for the layer.
  return (1u << 30) | ((bucket & 0x7Fu) << 23) | (layer & 0x007FFFFFu);
}
// Dynamic texture slot: source=2, bucket unused, low 24 bits hold the
// per-channel slot index. Consumer shaders branch on the source bits and sample
// one of a fixed set of direct sampler2D uniforms named <channel>Dyn0 and so on,
// forwarding the upstream handle as-is. For large runtime textures (8K video,
// HDR shader outputs) that do not fit the scaled-and-uploaded array path.
inline constexpr uint32_t tex_ref_dynamic(uint32_t slot)
{
  return (2u << 30) | (slot & 0x00FFFFFFu);
}

// Per-material data for the material SSBO, 80 bytes (5 x vec4). The 16 B of
// runtime metadata leaves headroom for future fields (animation ID, LOD hint,
// shader permutation hash) without another ABI break.
struct MaterialGPU
{
  float baseColor[4]{1.f, 1.f, 1.f, 1.f};
  // x = metallic, y = roughness, z = occlusion, w = unlit flag
  float metallicRoughnessOcclusionUnlit[4]{0.f, 0.5f, 1.f, 0.f};
  // xyz = emissive, w = emissive strength
  float emissive_strength[4]{0.f, 0.f, 0.f, 1.f};
  // Packed texture refs: [0] = base color, [1..3] reserved for MR, normal,
  // emissive. See tex_ref_* helpers for encoding.
  uint32_t textureRefs[4]{
      0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};

  // --- Runtime metadata (16 B) ----------------------------------------
  // Producer-derived bitmask of "which BRDF lobes / features are active"
  // for this material. Used as:
  //   - Coherence key for NVIDIA Shader Execution Reordering
  //     (`reorderThread(feature_mask)` before closest-hit shading) so
  //     threads in the same warp converge on the same shading path.
  //   - Shader-side specialization in the main closest-hit / fragment
  //     body: `if(fm & HAS_TRANSMISSION) { ... }`.
  // Bit layout:
  //   bit 0  : has_base_color_texture
  //   bit 1  : has_metal_rough_texture
  //   bit 2  : has_normal_texture
  //   bit 3  : has_emissive_texture
  //   bit 4  : unlit
  //   bit 5  : alpha_non_opaque (mask OR blend)
  //   bit 6  : has_clearcoat  (KHR_materials_clearcoat)
  //   bit 7  : has_sheen      (KHR_materials_sheen)
  //   bit 8  : has_transmission (KHR_materials_transmission)
  //   bit 9  : has_volume     (KHR_materials_volume)
  //   bit 10 : has_specular   (KHR_materials_specular)
  //   bit 11 : has_iridescence (KHR_materials_iridescence)
  //   bit 12 : has_anisotropy
  //   bit 13 : has_diffuse_transmission
  //   bit 14 : has_subsurface
  //   bit 15 : thin_walled
  //   bit 16 : alpha_mask                  (glTF alphaMode = MASK)
  //   bit 17 : alpha_blend                 (glTF alphaMode = BLEND)
  //   bit 18 : double_sided                (glTF doubleSided)
  //   bit 19 : has_separate_occlusion      (occlusion ≠ MR source)
  //   bits 20-21 : BC       texcoord_set (0 or 1, glTF TEXCOORD_0/1)
  //   bits 22-23 : MR       texcoord_set
  //   bits 24-25 : Normal   texcoord_set
  //   bits 26-27 : Emissive texcoord_set
  //   bits 28-29 : Occlusion texcoord_set
  //   bit 30 : shadow_caster_disabled      (material.shadow_caster == false)
  //   bit 31 : reflection_caster_disabled  (material.reflection_caster == false)
  uint32_t feature_mask{0u};

  // Shader binding table hit-group index for ray tracing pipelines.
  // Producers with a pre-computed hit-group mapping stamp this at
  // material-authoring time; 0 means "default lit material" and is the
  // safe fallback for renderers that haven't computed the mapping yet.
  uint32_t hit_group_id{0u};

  // 5th texture channel (occlusion). glTF separates occlusionTexture
  // from metallicRoughnessTexture; conventionally both are sometimes
  // packed into the same image (occlusion in R, roughness in G,
  // metallic in B). When they're distinct sources, this slot points
  // at the occlusion array layer; when they're the same, this stays
  // at tex_ref_none() and the shader uses MR.r * occlusion_factor.
  uint32_t occlusion_textureRef{0xFFFFFFFFu};

  // glTF alphaMode = MASK cutoff. Shader does `if(alpha < cutoff)
  // discard;` when the `alpha_mask` feature_mask bit is set.
  // Default 0.5 matches the glTF spec default.
  float alpha_cutoff{0.5f};
};
static_assert(sizeof(MaterialGPU) == 80, "MaterialGPU layout must match shader");

// Feature-mask bit flags. Producers OR these together to derive the
// per-material feature_mask; hit-group shaders branch on them to
// select the relevant BRDF lobe code path.
namespace material_feature
{
inline constexpr uint32_t has_base_color_texture   = 1u << 0;
inline constexpr uint32_t has_metal_rough_texture  = 1u << 1;
inline constexpr uint32_t has_normal_texture       = 1u << 2;
inline constexpr uint32_t has_emissive_texture     = 1u << 3;
inline constexpr uint32_t unlit                    = 1u << 4;
inline constexpr uint32_t alpha_non_opaque         = 1u << 5;
inline constexpr uint32_t has_clearcoat            = 1u << 6;
inline constexpr uint32_t has_sheen                = 1u << 7;
inline constexpr uint32_t has_transmission         = 1u << 8;
inline constexpr uint32_t has_volume               = 1u << 9;
inline constexpr uint32_t has_specular             = 1u << 10;
inline constexpr uint32_t has_iridescence          = 1u << 11;
inline constexpr uint32_t has_anisotropy           = 1u << 12;
inline constexpr uint32_t has_diffuse_transmission = 1u << 13;
inline constexpr uint32_t has_subsurface           = 1u << 14;
inline constexpr uint32_t thin_walled              = 1u << 15;
// glTF alpha mode (parsed from material.alphaMode). MASK → shader
// discards fragments with alpha < alpha_cutoff. BLEND → shader emits
// translucent alpha (caller handles depth/sort separately).
inline constexpr uint32_t alpha_mask               = 1u << 16;
inline constexpr uint32_t alpha_blend              = 1u << 17;
// glTF doubleSided. When set, shader flips the surface normal for
// back-facing fragments (so lighting works on both sides). When unset
// AND the pipeline cull mode is `none` (MDI default), shader discards
// back-facing fragments to mimic single-sided culling.
inline constexpr uint32_t double_sided             = 1u << 18;
// Separate occlusion texture present (independent from MR texture).
// Shader samples mat.occlusion_textureRef instead of using mr.r.
inline constexpr uint32_t has_separate_occlusion   = 1u << 19;
// Scene-filter opt-outs. "Disabled" semantics (default 0 = participates
// in the pass) so the common case stays bit-clear. Packed at bits
// 30/31 — CSF filter shaders test these to drop draws from auxiliary
// passes (shadow-map, reflection capture).
inline constexpr uint32_t shadow_caster_disabled     = 1u << 30;
inline constexpr uint32_t reflection_caster_disabled = 1u << 31;
}

// Per-material EXTENSION data: a parallel SSBO indexed by the same
// material_index as MaterialGPU. Shaders needing only the 64-byte base material
// ignore it; OpenPBR-grade shaders declare `scene_materials_ext`.
//
// std430-friendly: every member starts on a 16-byte boundary. Field names track
// OpenPBR_ResolvedInputs / glTF KHR extension names so the shader-side
// translation is a 1:1 copy.
//
// textureRefs[16] uses the same tex_ref_static / tex_ref_dynamic / tex_ref_none
// encoding as MaterialGPU.textureRefs. The slot ordering documented below MUST
// match what packMaterialExtensions writes and what the consumer shader's
// Material_Ext struct reads.
struct MaterialExtensionsGPU
{
  // --- Coat / clearcoat (KHR_materials_clearcoat) ---------------------
  // x = coat_weight, y = coat_roughness, z = coat_ior, w = coat_darkening
  float coat[4]{0.f, 0.f, 1.5f, 0.f};
  // x = roughness_anisotropy, y = rotation_cos, z = rotation_sin, w = _pad
  float coat_anisotropy[4]{0.f, 1.f, 0.f, 0.f};

  // --- Fuzz / sheen (KHR_materials_sheen) -----------------------------
  // xyz = color, w = roughness
  float fuzz_color[4]{0.f, 0.f, 0.f, 0.f};

  // --- Transmission + volume (KHR_materials_transmission + _volume) ---
  // x = transmission_weight, y = transmission_depth,
  // z = dispersion_scale,    w = dispersion_abbe_number
  float transmission[4]{0.f, 0.f, 0.f, 20.f};
  // xyz = transmission_color, w = scatter_anisotropy
  float transmission_color[4]{1.f, 1.f, 1.f, 0.f};
  // xyz = transmission_scatter (vec3), w = _pad
  float transmission_scatter[4]{0.f, 0.f, 0.f, 0.f};

  // --- Specular (KHR_materials_specular) + base specular anisotropy ---
  // x = specular_weight, yzw = specular_color
  float specular_weight_color[4]{1.f, 1.f, 1.f, 1.f};
  // x = specular_ior,   y = roughness_anisotropy,
  // z = rotation_cos,   w = rotation_sin
  float specular_ior_anisotropy[4]{1.5f, 0.f, 1.f, 0.f};

  // --- Thin-film iridescence (KHR_materials_iridescence) --------------
  // x = thin_film_weight (iridescence factor),
  // y = thin_film_thickness (glTF average of min/max),
  // z = thin_film_ior, w = _pad
  float thin_film[4]{0.f, 400.f, 1.3f, 0.f};

  // --- Diffuse transmission (KHR_materials_diffuse_transmission) ------
  // x = factor, yzw = color
  float diffuse_transmission[4]{0.f, 1.f, 1.f, 1.f};

  // --- Subsurface (OpenPBR subsurface; not present in stock glTF) -----
  // x = weight, yzw = color
  float subsurface_weight_color[4]{0.f, 0.8f, 0.8f, 0.8f};
  // x = radius, yzw = radius_scale
  float subsurface_radius_scale[4]{1.f, 1.f, 0.5f, 0.25f};

  // --- Misc scalars + flags -------------------------------------------
  // x = base_diffuse_roughness (OpenPBR Oren-Nayar knob),
  // y = thin_walled (bool-as-float 0/1),
  // z = _pad, w = _pad
  float flags[4]{0.f, 0.f, 0.f, 0.f};

  // --- Texture refs ---------------------------------------------------
  // Slot layout:
  //   0  = coat factor
  //   1  = coat roughness
  //   2  = coat normal
  //   3  = fuzz color (sheen)
  //   4  = fuzz roughness
  //   5  = transmission
  //   6  = specular factor
  //   7  = specular color
  //   8  = iridescence (thin-film)
  //   9  = iridescence thickness
  //   10 = anisotropy
  //   11 = diffuse transmission
  //   12 = diffuse transmission color
  //   13 = subsurface factor
  //   14 = subsurface color
  //   15 = reserved
  uint32_t textureRefs[16]{
      0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
      0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
      0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
      0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
};

// ─── Raw layouts (source-owned arena slots) ────────────────────────────
//
// Written by source halp nodes directly into their GpuResourceRegistry arena
// slot. ScenePreprocessor reads these, applies aspect-ratio and scene-graph
// composition, and writes the cooked equivalents consumer shaders bind.

// Camera parameters before matrix composition. No aspect ratio, no
// view / projection matrices — the preprocessor builds those per render
// target.
struct RawCameraData
{
  float eye[4]{0.f, 0.f, 3.f, 0.f};      // xyz = world-space eye, w = pad
  float target[4]{0.f, 0.f, 0.f, 0.f};   // xyz = look-at target,  w = pad
  float up[4]{0.f, 1.f, 0.f, 0.f};       // xyz = up,              w = pad
  float yfov{60.f * 3.14159265f / 180.f}; // vertical FOV, radians
  float znear{0.1f};
  float zfar{1000.f};
  uint32_t projection{0};                // 0 = perspective, 1 = orthographic
};

// Light parameters in local frame. The final world-space direction
// depends on the node's world transform (composed by the preprocessor
// from its scene-node parent chain); this struct stores only what the
// node itself knows.
struct RawLightData
{
  float color[4]{1.f, 1.f, 1.f, 1.f};          // xyz = color, w = intensity
  float local_direction[4]{0.f, 0.f, -1.f, 0.f}; // xyz = dir (local),
                                                  // w = type enum:
                                                  //   0 = directional
                                                  //   1 = point
                                                  //   2 = spot
                                                  // (area / dome modes
                                                  // collapse to point /
                                                  // directional; dome
                                                  // lights are served by
                                                  // the scene-global env
                                                  // path, see EnvParamsUBO.)
  float range_cone[4]{                          // x = range (point/spot;
      0.f, 1.f, 0.7071f, 0.005f};                //     0 = infinite)
                                                  // y = inner cone cos
                                                  // z = outer cone cos
                                                  // w = shadow depth bias
  // Shadow gate: shadow-receiving shaders MUST multiply the computed shadow term
  // by `shadow_enabled != 0`. Separate from the per-material
  // shadow_caster_disabled bit, which controls whether a draw participates in the
  // depth-only cast pass.
  uint32_t shadow_enabled{0};
  uint32_t decay_mode{2};                        // 0=const 1=lin 2=quad 3=cubic
  // RawTransform arena slot index for this light's scene_transform.
  // Consumer shader reads world_transforms.data[transform_slot] to
  // get the world matrix, composes world-space direction / position
  // from local_direction on the fly.
  uint32_t transform_slot{0};
  // Receiver-plane / slope-scaled bias for shadow sampling, exposed in the UI
  // as Light::inputs.shadow_normal_bias. PCF shaders add
  // `normal_bias * (1 - max(dot(N, Ldir), 0))` to the receiver depth before the
  // comparison, to kill shadow acne on grazing surfaces.
  float normal_bias{0.01f};
};
static_assert(sizeof(RawLightData) == 64, "RawLightData must stay 64 B");

// Local TRS for a scene_transform, stamped by the producer and uploaded into a
// RawTransform arena slot. Parent-chain composition stays on the CPU inside
// ScenePreprocessor's FlattenVisitor; the composed world matrix ends up in the
// WorldTransform arena at the same offset the RawTransform slot occupies.
struct RawLocalTransform
{
  float translation[4]{0.f, 0.f, 0.f, 0.f}; // xyz + pad
  float rotation[4]{0.f, 0.f, 0.f, 1.f};    // quaternion xyzw
  float scale[4]{1.f, 1.f, 1.f, 0.f};       // xyz + pad
  float _pad[4]{};                           // std430 alignment
};

// Environment parameters (ambient, fog, exposure, gamma). Already
// view-independent — this is both Raw (source-written) and Cooked
// (shader-bound) in one struct. Published here so EnvironmentLoader
// can write its own slot bytes matching what ScenePreprocessor expects
// on the other end.
struct EnvParamsUBO
{
  float ambient[4]{0.03f, 0.03f, 0.03f, 1.f};        // xyz = color, w = intensity
  float fog_color_density[4]{0.8f, 0.8f, 0.8f, 0.f}; // xyz = color, w = density
  float fog_range[4]{10.f, 100.f, 0.f, 0.f};         // x = start, y = end,
                                                      // z = mode, w = enabled (0/1)
  float exposure_gamma[4]{1.f, 2.2f, 0.f, 0.f};      // x = exposure (linear),
                                                      // y = gamma, zw = pad
};

// World-space mat4 emitted by ScenePreprocessor's FlattenVisitor from
// the scene_node tree (CPU walk with parent-chain accumulation). One
// entry per producer-authored scene_transform, laid out at the same
// byte offset as the producer's RawTransform slot so shaders can
// address either side by `scene_transform::raw_slot.offset`.
struct WorldTransformMat4
{
  float m[16]{1.f, 0.f, 0.f, 0.f,
              0.f, 1.f, 0.f, 0.f,
              0.f, 0.f, 1.f, 0.f,
              0.f, 0.f, 0.f, 1.f};
};

// Shadow cascades UBO, scene-wide, published by ScenePreprocessor as the
// `shadow_cascades` aux on the output geometry. Shading shaders read it to pick
// the right cascade per fragment; the depth-only pass reads light_view_proj from
// it to transform vertices into cascade clip-space, with its per-invocation
// cascade_index in a separate `shadow_draw_cfg` UBO.
//
// std140, 560 B. Fields mirror ossia::shadow_cascades_info in geometry_port.hpp:
//   light_view_proj[8]         world -> cascade clip-space per cascade
//   cascade_split_distances[8] view-space far-plane Z of cascade k; slots
//                              >= cascade_count read as 0
//   cascade_count              how many entries are live (0..8)
struct ShadowCascadesUBO
{
  float light_view_proj[8][16]{};
  // 8 split distances symmetric with light_view_proj[8].
  // std140: two consecutive vec4 rows (32 B total).
  float cascade_split_distances[8]{};
  uint32_t cascade_count{0};
  uint32_t _pad0{};
  uint32_t _pad1{};
  uint32_t _pad2{};
};
static_assert(sizeof(ShadowCascadesUBO) == 560,
              "ShadowCascadesUBO size = mat4[8] (512) + float[8] (32) + 4×uint (16) = 560 B");

#pragma pack(pop)

// CPU-side flattened scene representation.
struct DrawCall
{
  // Points at either a mesh from geometry_ref (legacy_geometry path) OR at
  // owned_mesh (mesh_primitive path). `mesh` is always non-null for a valid
  // draw; one of geometry_ref or owned_mesh keeps the target alive.
  const ossia::geometry* mesh{};
  ossia::geometry_spec geometry_ref;            // Legacy path: keeps source alive.
  std::shared_ptr<ossia::geometry> owned_mesh;  // Primitive path: built from mesh_primitive.

  // Stable cross-frame identity of the source mesh primitive: the mesh_primitive's
  // stable_id, or its raw pointer bits for a legacy producer that stamps none.
  // `mesh` cannot serve -- on the primitive path it points into a freshly
  // allocated wrapper with a different address every flatten. ScenePreprocessor
  // uses this to skip vertex/index re-uploads.
  uint64_t stable_id{};

  QMatrix4x4 worldTransform;

  // Direct shared_ptr to the material — null means "no material / use
  // the renderer's default factors". Carries the material's gpu_slot_ref
  // for GPU-side lookup without any scene-wide index array.
  ossia::material_component_ptr material;

  // Direct shared_ptr to the skin — null means "no skinning". When
  // present, the ScenePreprocessor attaches a `joint_matrices` auxiliary
  // buffer to this draw's output geometry; a downstream skinning compute
  // pass (or user shader) deforms positions/normals using
  // joints0/weights0 vertex attributes.
  ossia::skeleton_component_ptr skin;

  // Index into FlatScene::materials after the flatten pass has
  // deduplicated the material pointers into its flat materials array.
  // -1 means "material was null / default factors only". Set by
  // flattenScene after collecting all draws.
  int materialIndex{-1};

  // Index into FlatScene::skins after dedup. -1 = no skinning.
  int skinIndex{-1};

  // Local-space AABB of the source mesh_primitive. Copied by the
  // FlattenVisitor from mesh_primitive::bounds. Empty (inverted) if the
  // source didn't compute bounds — downstream per_draw_bounds emitter
  // writes an infinite AABB in that case so GPU culling shaders never
  // cull the draw.
  ossia::aabb local_bounds{};

  // RawTransform arena slot of the nearest producer-authored
  // scene_transform on this draw's walk path (0xFFFFFFFF = none). Stamped
  // into PerDrawGPU.transform_slot so shaders can look up
  // world_transforms_prev[slot] for motion vectors / TAA / reprojection.
  std::uint32_t transform_slot{0xFFFFFFFFu};
};

// Per-skeleton packed joint matrices: joint_matrix[i] = world_joint × inverse_bind.
// One std::vector<QMatrix4x4> per skeleton index (parallel to scene_state.skeletons).
struct SkeletonGPU
{
  std::vector<QMatrix4x4> joint_matrices;
};

// One entry per producer-authored scene_transform seen during the walk. The
// preprocessor's private world-transforms SSBO is a packed array indexed by the
// scene_transform's raw_slot.internal_index, and consumer shaders read
// world_transforms.data[transform_slot].
//
// Each preprocessor owns its own buffer, so two preprocessors with different
// filtered views of one scene compute different world matrices for the same
// scene_transform without stomping each other.
struct WorldTransformEmit
{
  QMatrix4x4 world;
  uint32_t transform_slot;  // RawTransform arena slot index
};

struct FlatScene
{
  std::vector<DrawCall> draws;
  // RawLight arena slot index per light the walk encountered.
  // 0xFFFFFFFF for producer-less lights (filtered out when building
  // scene_light_indices, the shader-facing compact indices list).
  std::vector<uint32_t> lightArenaSlots;
  std::vector<MaterialGPU> materials;
  // Parallel to `materials`: same size, same indexing, zeroed to the OpenPBR spec
  // defaults for materials that set no extension fields. Consumer shaders either
  // ignore it or bind it as `scene_materials_ext`.
  std::vector<MaterialExtensionsGPU> material_extensions;
  std::vector<SkeletonGPU> skins;  // Parallel to scene_state.skeletons.

  // World matrices to upload into the WorldTransform arena, one per
  // producer-authored scene_transform encountered in the walk whose
  // raw_slot is valid. Sparse: the arena is indexed by offset, not
  // by position in this vector.
  std::vector<WorldTransformEmit> worldTransforms;

  // Loader-emitted scene_data payloads, collected during the walk.
  // ScenePreprocessor forwards each entry as an auxiliary_buffer on every output
  // geometry (by name). Lifetime held via shared_ptr.
  std::vector<ossia::scene_data_ptr> scene_data;

  // Instance components encountered during the walk. Each pair is a
  // (worldTransform, instance_component_ptr) that the ScenePreprocessor emits as
  // a dedicated instanced DrawCall with per-instance auxiliaries.
  struct InstanceDraw
  {
    ossia::instance_component_ptr instance;
    QMatrix4x4 worldTransform;
  };
  std::vector<InstanceDraw> instances;

  // Primitive cloud (splat / point-cloud) entries: format-agnostic payloads whose
  // schema is described by their CSF chain. ScenePreprocessor buckets them by
  // format_id, one indirect-draw geometry per bucket; an empty format_id is
  // bucketed individually on the stable id.
  struct PrimitiveCloudDraw
  {
    ossia::primitive_cloud_component_ptr cloud;
    QMatrix4x4 worldTransform;
    // RawTransform arena slot index, or 0xFFFFFFFFu if no producer
    // transform was on the walk path. Mirrors PerDrawGPU.transform_slot.
    uint32_t transform_slot{0xFFFFFFFFu};
  };
  std::vector<PrimitiveCloudDraw> primitive_clouds;

  // Cameras collected from the scene tree. Each entry keeps its source
  // camera_component alive, its accumulated world transform (column 3 =
  // eye position, inverse = view matrix), and the scene_node_id of the
  // node it was attached to so consumers can resolve `active_camera_id`.
  struct CameraEntry
  {
    ossia::camera_component_ptr component;
    QMatrix4x4 worldTransform;
    ossia::scene_node_id node_id{};
  };
  std::vector<CameraEntry> cameras;

  // Index into `cameras` of the currently-active camera. -1 when the scene
  // has no cameras; in that case downstream falls back to a default eye
  // placement (see the legacy single-camera fields below, populated from
  // this slot if valid or from a default otherwise).
  int activeCameraIndex{-1};

  // Camera (from scene or override) — legacy mirror fields. Kept populated
  // for consumers that haven't migrated to `cameras[activeCameraIndex]`
  // yet. Resolved by flattenScene() after the tree walk:
  //   - cameras empty   → sensible default (eye at (0,1,3))
  //   - cameras nonempty → copied from cameras[activeCameraIndex]
  QMatrix4x4 viewMatrix;
  QMatrix4x4 projectionMatrix;
  QVector3D cameraPosition;
  float cameraFov{60.f};
  float cameraNear{0.1f};
  float cameraFar{1000.f};

  bool hasCamera{false};

  void clear()
  {
    draws.clear();
    lightArenaSlots.clear();
    materials.clear();
    material_extensions.clear();
    skins.clear();
    scene_data.clear();
    instances.clear();
    primitive_clouds.clear();
    cameras.clear();
    worldTransforms.clear();
    activeCameraIndex = -1;
    hasCamera = false;
    // The legacy single-camera mirror is part of the state too: leaving it
    // populated means a reused FlatScene reports the previous scene's camera
    // to any consumer that reads it without checking hasCamera.
    viewMatrix.setToIdentity();
    projectionMatrix.setToIdentity();
    cameraPosition = {};
    cameraFov = 60.f;
    cameraNear = 0.1f;
    cameraFar = 1000.f;
  }
};

// Flatten a scene_spec into a FlatScene for GPU consumption.
void flattenScene(
    const ossia::scene_spec& scene,
    FlatScene& out,
    float aspectRatio);

// Build a transient ossia::geometry that wraps a mesh_primitive's buffers
// and attributes. The result is heap-allocated and owned by shared_ptr so
// callers can keep it alive beyond the flatten pass (see DrawCall::owned_mesh).
std::shared_ptr<ossia::geometry>
primitiveToGeometry(const ossia::mesh_primitive& prim);

MaterialGPU packMaterial(const ossia::material_component& mc);
MaterialExtensionsGPU packMaterialExtensions(const ossia::material_component& mc);
}
