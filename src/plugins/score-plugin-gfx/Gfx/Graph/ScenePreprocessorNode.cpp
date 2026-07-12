#include "Gfx/Graph/GpuResourceRegistry.hpp"

#include <Gfx/AssetTable.hpp>
#include <Gfx/Graph/CameraMath.hpp>
#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RhiClearBuffer.hpp>
#include <Gfx/Graph/RhiComputeBarrier.hpp>
#include <Gfx/Graph/SceneGPUState.hpp>
#include <Gfx/Graph/ScenePreprocessorNode.hpp>
#include <Gfx/Graph/TextureLoader.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QByteArray>
#include <QImage>
#include <QQuaternion>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>

namespace score::gfx
{

namespace
{

// std430 layout matching the `per_draw` AUXILIARY block declared in the
// preset rasterizer shaders. Lays down model + normal matrices, the
// material index, and a 32-bit tag hash (rapidhash of material.tag,
// truncated to 32 bits — same primitive that produces filter_tag and
// content_hash everywhere else in the pipeline) for downstream
// per-pass filtering.
//
// `transform_slot` indexes into the `world_transforms` /
// `world_transforms_prev` SSBOs — motion-vector / TAA / reprojection
// shaders do `world_transforms_prev.data[pd.transform_slot]` to recover
// the previous-frame world matrix of this draw. 0xFFFFFFFF = no
// producer-authored transform on the walk path (draw anchored to the
// identity or a loader-interior transform); shaders must treat this as
// "motion = zero" / "no prev data".
//
// `skeleton_offset` is the offset (in joint-matrix units) where this
// draw's skeleton begins inside a consolidated joint_matrices buffer.
// 0xFFFFFFFF = unskinned draw. Today joint_matrices is bound per-draw
// and the offset is functionally always 0 for skinned draws, but we
// stamp the correct concat-offset here so a future consolidation that
// switches to a single arena-style joint_matrices SSBO does not need a
// PerDrawGPU layout change.
struct PerDrawGPU
{
  float model[16]{};
  float normal[16]{};   // mat3 padded as mat4 to keep std430 alignment trivial
  uint32_t material_index{};
  uint32_t tag_hash{};
  uint32_t transform_slot{0xFFFFFFFFu};
  uint32_t skeleton_offset{0xFFFFFFFFu};
};
static_assert(sizeof(PerDrawGPU) == 144, "PerDrawGPU layout must match shader");

// Local-space AABB per draw. Emitted as the `per_draw_bounds` auxiliary
// SSBO (sidecar to `per_draws`, same indexing by drawID / gl_BaseInstance).
// Consumer shaders transform to world space via Arvo's algorithm against
// PerDrawGPU.model and test against the camera's frustum planes for
// GPU frustum / HiZ occlusion culling.
//
// Sentinel convention: when the source mesh didn't compute bounds, we
// emit an "infinite" AABB (min = -FLT_MAX, max = +FLT_MAX) so culling
// shaders leave the draw alone rather than degenerating to a point at
// the origin.
struct PerDrawBoundsGPU
{
  float aabb_min[4]{};  // xyz = local-space min, w = unused (padding)
  float aabb_max[4]{};  // xyz = local-space max, w = unused (padding)
};
static_assert(sizeof(PerDrawBoundsGPU) == 32,
              "PerDrawBoundsGPU layout must match shader (2 × vec4)");

// Pack an ossia::aabb into PerDrawBoundsGPU. Empty (inverted) input means
// the source mesh didn't compute bounds — emit a ±FLT_MAX "infinite" box
// so culling shaders never cull the draw. This keeps sources that can't
// easily supply bounds (GPU-resident procedural meshes like PBRMesh)
// rendering correctly through a cull pass.
inline PerDrawBoundsGPU packBounds(const ossia::aabb& b) noexcept
{
  PerDrawBoundsGPU g{};
  if(b.empty())
  {
    constexpr float kPos = std::numeric_limits<float>::max();
    constexpr float kNeg = -std::numeric_limits<float>::max();
    g.aabb_min[0] = kNeg; g.aabb_min[1] = kNeg; g.aabb_min[2] = kNeg;
    g.aabb_max[0] = kPos; g.aabb_max[1] = kPos; g.aabb_max[2] = kPos;
  }
  else
  {
    g.aabb_min[0] = b.min[0]; g.aabb_min[1] = b.min[1]; g.aabb_min[2] = b.min[2];
    g.aabb_max[0] = b.max[0]; g.aabb_max[1] = b.max[1]; g.aabb_max[2] = b.max[2];
  }
  return g;
}

// MaterialGPU = 4 × vec4 in the shader (baseColor, MR-occlusion-unlit,
// emissive_strength, textureRefs). Layout drift here silently corrupts
// every textured draw — keep the size check.
static_assert(sizeof(MaterialGPU) == 80, "MaterialGPU layout must match shader");

// Per-material per-channel UV transforms (KHR_texture_transform).
// 5 channels × (offset.xy + scale.xy) + rotations packed in 2 vec4
// = 7 vec4 = 112 B. Channels match MaterialChannel enum: 0=BC, 1=MR,
// 2=Normal, 3=Em, 4=Occlusion. Identity transform: offset=(0,0),
// scale=(1,1), rotation=0 — the default-constructed value, which
// makes glTFs without the extension pass through `(uv) → uv` and
// incur zero shader cost.
struct MaterialUVTransformGPU
{
  float bc_offset_scale[4]{0.f, 0.f, 1.f, 1.f};      // ox, oy, sx, sy
  float mr_offset_scale[4]{0.f, 0.f, 1.f, 1.f};
  float normal_offset_scale[4]{0.f, 0.f, 1.f, 1.f};
  float em_offset_scale[4]{0.f, 0.f, 1.f, 1.f};
  float occ_offset_scale[4]{0.f, 0.f, 1.f, 1.f};
  float rotations0[4]{0.f, 0.f, 0.f, 0.f};           // bc, mr, nrm, em (radians)
  float rotations1[4]{0.f, 0.f, 0.f, 0.f};           // occ, _pad×3
};
static_assert(sizeof(MaterialUVTransformGPU) == 112,
              "MaterialUVTransformGPU layout must match shader (7 × vec4)");

// Material texture channels. Each channel has its own QRhiTextureArray with
// the appropriate pixel format (sRGB vs linear) and dedup map. Index into
// MaterialGPU::textureRefs[].
enum MaterialChannel : int
{
  ChannelBaseColor = 0,
  ChannelMetalRough = 1,
  ChannelNormal = 2,
  ChannelEmissive = 3,
  ChannelOcclusion = 4,  // Separate glTF occlusionTexture (when distinct from MR).
  ChannelCount = 5
};

// Whole texture_ref for a given channel, or nullptr for out-of-range.
// Used by both the static path (reads .source) and the dynamic path
// (reads .texture.native_handle).
inline const ossia::texture_ref*
channelRef(MaterialChannel ch, const ossia::material_component& m) noexcept
{
  switch(ch)
  {
    case ChannelBaseColor:  return &m.base_color_texture;
    case ChannelMetalRough: return &m.metallic_roughness_texture;
    case ChannelNormal:     return &m.normal_texture;
    case ChannelEmissive:   return &m.emissive_texture;
    case ChannelOcclusion:  return &m.occlusion_texture;
    default:                return nullptr;
  }
}

// Shader-visible name for each channel — matches the INPUT entries consuming
// shaders declare (sampler2DArray baseColorArray; etc). Names follow the
// existing classic_pbr_textured convention (camelCase) so the aux-texture
// auto-resolve path slots in without shader edits.
inline const char* channelName(MaterialChannel ch) noexcept
{
  switch(ch)
  {
    case ChannelBaseColor:  return "baseColorArray";
    case ChannelMetalRough: return "metalRoughArray";
    case ChannelNormal:     return "normalArray";
    case ChannelEmissive:   return "emissiveArray";
    case ChannelOcclusion:  return "occlusionArray";
    default:                return "";
  }
}

// Dynamic-slot aux-texture name base. The full name is
// `<base><slot_index>` (e.g., "baseColorDyn0"), matching the uniform
// names consumer shaders declare for the dynamic branch.
inline const char* channelDynBaseName(MaterialChannel ch) noexcept
{
  switch(ch)
  {
    case ChannelBaseColor:  return "baseColorDyn";
    case ChannelMetalRough: return "metalRoughDyn";
    case ChannelNormal:     return "normalDyn";
    case ChannelEmissive:   return "emissiveDyn";
    case ChannelOcclusion:  return "occlusionDyn";
    default:                return "";
  }
}

// Authoritative kMaxDynamicSlots constant lives on
// GpuResourceRegistry::kMaxDynamicSlots (header). Removed the local
// duplicate that drifted out of sync; the registry value is what actually
// gates the dynamic-slot cap (see resolveDynamicSlot at line ~386 in
// GpuResourceRegistry.cpp).

// sRGB channels (base color, emissive) get hardware sRGB→linear on sample.
// Metallic-roughness and normal are data, not color — must stay linear.
inline QRhiTexture::Flags channelFlags(MaterialChannel ch) noexcept
{
  switch(ch)
  {
    case ChannelBaseColor:
    case ChannelEmissive:
      return QRhiTexture::sRGB;
    default:
      return {};
  }
}

// =============================================================================
// Ext-texture slot routing (KHR_materials_*)
// =============================================================================
//
// Each MaterialExtensionsGPU::textureRefs[slot] is fed by an ext texture from
// material_component, registered into one of the 5 existing channel pools
// (BaseColor / MetalRough / Normal). Pool choice = format expectation:
//   ChannelBaseColor  → sRGB color textures (sheen color, specular color,
//                       diffuse-transmission color, subsurface color)
//   ChannelMetalRough → linear scalar/factor textures (clearcoat factor +
//                       roughness, sheen roughness, transmission, specular
//                       factor, iridescence, diffuse-transmission factor,
//                       subsurface factor)
//   ChannelNormal     → tangent-space data (clearcoat normal, anisotropy
//                       direction)
//
// Slot numbering matches MaterialExtensionsGPU::textureRefs[] documented in
// SceneGPUState.hpp — they MUST stay in sync; this table is the loader-side
// counterpart of the shader-side switch (see classic_pbr_openpbr.frag).
//
// Slots 13/14 (subsurface factor / color) and 15 (reserved) are intentionally
// absent from this table: stock glTF has no SSS extension and material_
// component carries no source texture_ref to drive them. Future loaders
// growing `material_component::subsurface` fields can extend the table
// here — the rebuild + patch walkers iterate kExtTextureSlots without
// hard-coded slot count, so a single new entry is all it takes.
struct ExtTextureSlot
{
  int slot;                 // 0..15 in MaterialExtensionsGPU::textureRefs
  MaterialChannel channel;  // which existing pool this texture lands in
  // Accessor returns a reference into `m`'s ext struct; the caller does
  // its `valid()` / `source.get()` test on the resulting texture_ref.
  // Returning by reference avoids dangling on temporary structs the
  // accessor would have to construct otherwise.
  const ossia::texture_ref& (*accessor)(const ossia::material_component& m);
};

inline constexpr ExtTextureSlot kExtTextureSlots[] = {
    // KHR_materials_clearcoat — slots 0..2.
    { 0,  ChannelMetalRough,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.clearcoat.texture; } },
    { 1,  ChannelMetalRough,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.clearcoat.roughness_texture; } },
    { 2,  ChannelNormal,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.clearcoat.normal_texture; } },

    // KHR_materials_sheen — slots 3..4.
    { 3,  ChannelBaseColor,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.sheen.color_texture; } },
    { 4,  ChannelMetalRough,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.sheen.roughness_texture; } },

    // KHR_materials_transmission — slot 5.
    { 5,  ChannelMetalRough,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.transmission.texture; } },

    // KHR_materials_specular — slots 6..7.
    { 6,  ChannelMetalRough,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.specular.texture; } },
    { 7,  ChannelBaseColor,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.specular.color_texture; } },

    // KHR_materials_iridescence — slots 8..9.
    { 8,  ChannelMetalRough,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.iridescence.texture; } },
    { 9,  ChannelMetalRough,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.iridescence.thickness_texture; } },

    // KHR_materials_anisotropy — slot 10.
    { 10, ChannelNormal,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.anisotropy.texture; } },

    // KHR_materials_diffuse_transmission — slots 11..12.
    { 11, ChannelMetalRough,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.diffuse_transmission.texture; } },
    { 12, ChannelBaseColor,
      +[](const ossia::material_component& m) -> const ossia::texture_ref& {
          return m.diffuse_transmission.color_texture; } },
};

QMatrix4x4 transformToMatrix(const ossia::scene_transform& t)
{
  QMatrix4x4 mat;
  mat.translate(t.translation[0], t.translation[1], t.translation[2]);
  mat.rotate(QQuaternion(t.rotation[3], t.rotation[0], t.rotation[1], t.rotation[2]));
  mat.scale(t.scale[0], t.scale[1], t.scale[2]);
  return mat;
}

// writeMat4 comes from Gfx/Graph/CameraMath.hpp (included above) — same
// signature, column-major memcpy. Keeping a local copy would create an
// ambiguous overload at every call site.

}

struct RenderedScenePreprocessorNode final : NodeRenderer
{
  // Texture arrays now live in GpuResourceRegistry and are destroyed
  // by RenderList::release → registry.destroy(). Nothing to clean up
  // here — the destructor is defaulted.

  const ScenePreprocessorNode& m_node;

  // Output owned GPU buffers (one set per flatten cycle). Sized to scene needs.
  // scene_light_indices SSBO: compact list of RawLight arena slot
  // indices for the current scene's live lights. Shader iterates
  // 0..scene_counts.light_count and reads
  // scene_lights.entries[scene_light_indices.data[i]].
  QRhiBuffer* m_lightIndicesBuffer{};
  int64_t m_lightIndicesCap{};
  std::vector<uint32_t> m_cachedLightIndices;
  // scene_materials is now served by the Material arena directly
  // (registry.buffer(Arena::Material)) — no preprocessor-owned mirror.
  // MaterialExtensions stays preprocessor-owned pending its own arena
  // migration (larger struct, less pressure to move).
  QRhiBuffer* m_materialsExtBuffer{};  // MaterialExtensionsGPU[]
  // KHR_texture_transform: per-material per-channel UV offset/scale/
  // rotation. Parallel to scene_materials, indexed by material_index.
  // Identity for materials without the extension (zero shader cost).
  QRhiBuffer* m_materialUVTransformsBuffer{};
  int64_t m_materialUVTransformsCap{};
  std::vector<MaterialUVTransformGPU> m_cachedMaterialUVTransforms;

  // One QRhiBuffer per forwarded scene_data entry — allocated when the
  // scene_data carries CPU-side `buffer_data`, borrowed from the upstream
  // when it already holds a `gpu_buffer_handle`. Parallel to fs.scene_data.
  struct SceneDataBinding
  {
    QRhiBuffer* buffer{};
    std::string name;
    int64_t byte_size{};
    bool owned{false};
  };
  std::vector<SceneDataBinding> m_sceneDataBuffers;

  // One per skeleton in scene_state.skeletons, holding the packed
  // joint_matrices (mat4[N]). Grow-only; skinned draws attach one of these
  // as a `joint_matrices` auxiliary.
  struct SkinBinding
  {
    QRhiBuffer* buffer{};
    int64_t capacity{};
    int64_t byte_size{};
  };
  std::vector<SkinBinding> m_skinBuffers;

  // std140-packed counts UBO: shaders read `scene_counts.light_count`,
  // `.material_count`, `.draw_count` instead of `scene_lights.entries
  // .length()`, so the SSBOs can keep their growth-only capacity without
  // forcing shaders to iterate ghost tail entries. Uploaded on every
  // change (partial uploads to scene_lights etc. may leave dead tail
  // slots when counts shrink, and we want the shader to ignore them).
  struct SceneCountsUBO
  {
    uint32_t light_count{};
    uint32_t material_count{};
    uint32_t draw_count{};
    uint32_t _pad0{};
  };
  static_assert(sizeof(SceneCountsUBO) == 16, "scene_counts UBO layout");
  QRhiBuffer* m_sceneCountsBuffer{};
  SceneCountsUBO m_cachedSceneCounts{~0u, ~0u, ~0u, 0u};

  // `shadow_cascades` aux UBO — light_view_proj[8] + split distances +
  // cascade_count. Populated from `scene.state->shadow_cascades` (authored
  // upstream by ShadowCascadeSetup). Diff-uploaded against the cached
  // snapshot; unchanged frames cost zero bytes. Emitted to downstream as
  // an `auxiliary_buffer` named "shadow_cascades" — classic_pbr_shadowed
  // reads it to PCF-sample the right cascade; shadow_cascades.vert reads
  // its `light_view_proj` array to transform vertices into cascade
  // clip-space (its per-invocation `cascade_index` lives in a separate
  // `shadow_draw_cfg` UBO that the depth-pass pipeline binds locally).
  QRhiBuffer* m_shadowCascadesBuffer{};
  ShadowCascadesUBO m_cachedShadowCascades{};
  bool m_shadowCascadesSeeded{false};

  // Per-camera std140 UBO array. Size = max(1, ncameras) * sizeof(CameraUBOData).
  // First entry is always the active camera (resolved by flattenScene from
  // scene_state.active_camera_id). When the scene has no cameras we publish
  // a single default entry so the shader never sees a null binding.
  // Bound as the `camera` aux buffer on Geometry Out — try_bind_from_geometry
  // in the shader consumer resolves it by port name.
  QRhiBuffer* m_camerasBuffer{};
  int64_t m_camerasCap{};
  std::vector<CameraUBOData> m_cachedCameras;

  // One-frame history for motion-vector reprojection. Bound as the aux UBO
  // `camera_prev`; consumer post-process shaders reconstruct world position
  // from current depth + current camera, then reproject through this.
  // On the first frame (no history) we seed prev = current so MV = 0.
  // Filled each frame from m_cachedCameras BEFORE m_camerasBuffer is
  // overwritten — same "GPU snapshot of last frame" semantics as
  // m_worldTransformsPrevBuffer, just on a Dynamic UBO via CPU shadow
  // upload instead of copyBuffer (which Dynamic UBOs don't support).
  QRhiBuffer* m_camerasPrevBuffer{};

  // Per-frame guard for packAndUploadCameras. update() is invoked once
  // per outgoing edge by RenderList::renderInternal — for a
  // ScenePreprocessor with N consumers, that's N calls per frame. The
  // camera-prev semantic ("upload m_cachedCameras BEFORE overwriting
  // it with fresh") only holds on the first call; on the second call,
  // m_cachedCameras has already been replaced by fresh, so re-running
  // would clobber camera_prev with current camera content.
  // Keep packAndUploadCameras idempotent within a frame by tracking
  // the last frame index we ran on (RenderList::frame, incremented at
  // the end of each renderInternal). -1 = not yet run.
  int64_t m_lastCameraUploadFrame{-1};

  // Per-preprocessor world-transforms SSBO. One WorldTransformMat4 per
  // producer-authored scene_transform seen during the walk, laid out in
  // walk order. Not a shared registry arena — different preprocessors
  // consuming different filtered views of the same source scene
  // legitimately compute different world matrices for the same
  // scene_transform, so each keeps its own buffer. Consumer shaders
  // bind `world_transforms` by aux name and index via
  // `per_draws[draw_id].transform_slot`.
  QRhiBuffer* m_worldTransformsBuffer{};
  int64_t m_worldTransformsCap{0};

  // Previous-frame snapshot of m_worldTransformsBuffer. Bound as the
  // `world_transforms_prev` aux buffer on Geometry Out; consumer
  // shaders read it alongside `world_transforms` for motion-vector /
  // TAA / reprojection passes. Maintained by a deferred-write scheme:
  // update() stashes this frame's per-slot WorldTransformMat4 writes
  // into m_pendingWorldXformWrites WITHOUT touching the resource-
  // update batch. runInitialPasses then (a) issues a single GPU-side
  // copyBuffer(current → prev) on the command buffer — at this point
  // current still holds frame-N-1 data because the deferred writes
  // haven't been applied yet — then (b) drains the pending list into
  // the next resource-update batch (`res`), which RenderList submits
  // AFTER runInitialPasses returns. Net: prev captures frame N-1's
  // state, current then receives frame N's writes; consumer render
  // passes downstream see the correct (prev, current) pair.
  // Same Static + StorageBuffer constraint as the current buffer
  // (QRhi forbids Dynamic + StorageBuffer).
  QRhiBuffer* m_worldTransformsPrevBuffer{};

  // Per-slot world-transform writes deferred from update() to
  // runInitialPasses so that the prev-snapshot copy captures frame
  // N-1 data before frame N's writes overwrite current. Drained once
  // per frame, gated by m_lastSnapshotFrame.
  std::vector<std::pair<uint32_t, WorldTransformMat4>>
      m_pendingWorldXformWrites;
  // Single-fire-per-frame guard for the prev-snapshot + pending-writes
  // drain. runInitialPasses is invoked once per outgoing edge, so without
  // a gate the snapshot would queue N copies and the pending-writes drain
  // would double-upload. We compare against renderer.frame (the monotonic
  // per-renderer frame counter that the camera path also uses, see the
  // packAndUploadCameras / camera prev-snapshot sites). NB: the previous
  // QRhiCommandBuffer-pointer discriminator was broken — every QRhi
  // backend (Vulkan/D3D11/D3D12/Metal/GL) returns the address of a single
  // by-value cbWrapper member from QRhiSwapChain::currentFrameCommandBuffer,
  // so the pointer is constant across frames and the gate fired exactly
  // once per swapchain lifetime, freezing world_transforms / _prev at
  // their frame-0 contents (motion vectors / TAA / reprojection broken).
  // Cleared on teardown (see release()).
  int64_t m_lastSnapshotFrame{-1};

  // Single-fire-per-frame guard for issuePendingGpuCopies.
  // runInitialPasses fires once per outgoing edge; without a gate a node
  // feeding K consumers issues K identical copy batches per frame (the
  // destination MDI buffers are shared, so one batch already serves every
  // consumer). Kept separate from m_lastSnapshotFrame because the snapshot
  // block only sets that token when the world-transforms buffer exists —
  // a dedicated token gates the copies unconditionally. Cleared on teardown.
  int64_t m_lastGpuCopiesFrame{-1};

  // Environment params UBO: preprocessor-owned Env arena slot. Each
  // EnvironmentLoader / CubemapLoader contributes disjoint fields (via
  // `params_set` bits on scene_environment); merge_scenes composes them
  // field-by-field into this->scene.state->environment. The preprocessor
  // packs the MERGED CPU-side env into m_envSlot here so consumers
  // reading `env` see the composed result, not any one producer's
  // contribution. The per-producer Env slots owned by EnvironmentLoader
  // etc. remain valid but are no longer the binding target — they're
  // just CPU-side marker that the producer is participating.
  GpuResourceRegistry::Slot m_envSlot{};
  uint32_t m_env_aux_offset{0};
  // Cache the last uploaded EnvParamsUBO bytes so we can skip re-upload
  // when the merged environment content doesn't change frame-to-frame.
  EnvParamsUBO m_lastEnvUpload{};
  bool m_envSlotSeeded{false};

  // ─── MDI state ───────────────────────────────────────────────────────
  // Post-migration, the vertex/index streams live in the registry's
  // MeshArenaManager. Only per_draws + indirect_draw_cmds remain
  // preprocessor-owned — they're small, scene-wide SSBOs tied to a
  // specific preprocessor's filtered view of the scene and not
  // shareable across preprocessors.
  struct MDIState
  {
    QRhiBuffer* per_draws{};
    QRhiBuffer* indirect_draw_cmds{};
    // Sidecar bounds SSBO parallel to per_draws. Same draw indexing
    // (baseInstance / gl_BaseInstance), read by GPU culling shaders to
    // transform local-space AABBs to world space and test against the
    // camera frustum.
    QRhiBuffer* per_draw_bounds{};
    int64_t perDrawsCap{};
    int64_t indirectCap{};
    int64_t perDrawBoundsCap{};
    uint32_t totalVertices{};
    uint32_t totalIndices{};
    uint32_t drawCount{};
  };
  MDIState m_mdi;

  // ─── Primitive cloud (splat) bucket resources ───────────────────────
  // One entry per bucket_key (hash(format_id) — or stable_id when
  // format_id is empty so each unformatted cloud gets its own bucket).
  // Each bucket carries:
  //   - raw_splats: concatenation of all clouds' raw_data in the bucket
  //   - cloud_meta: CloudMetaGPU[] (model matrix + slot indices)
  //   - cloud_id_lookup: uint per primitive -> cloud_meta index
  //   - indirect: a single IndirectCmd {6, total_primitives, 0, 0, 0}
  //
  // Buffers are persistent (growBuf-managed) so downstream SRBs see
  // pointer-stable handles across frames. A bucket whose key disappears
  // from the next flatten gets dropBuf'd in releaseStaleClouds().
  //
  // CloudMetaGPU mirrors PerDrawGPU's pattern (model[16] +
  // transform_slot) so a CSF chain that wants per-cloud TRS reads it
  // exactly the same way mesh shaders read per_draws[gl_DrawID].
  //
  // bounds_min / bounds_max are the per-cloud world-space AABB —
  // populated by walking the 8 corners of `cloud->bounds` through
  // `worldTransform`. Splat-format CSFs use these to do a per-cloud
  // frustum-cull pre-pass so off-screen clouds skip all per-primitive
  // work (a big win when scenes carry many bucketed clouds).
  struct CloudMetaGPU
  {
    float model[16];                 // 64
    float bounds_min[4];             // 80   xyz + pad
    float bounds_max[4];             // 96   xyz + pad
    uint32_t primitive_offset;       // 100
    uint32_t primitive_count;        // 104
    uint32_t transform_slot;         // 108
    uint32_t format_param_index;     // 112
    uint32_t _pad[4];                // 128 — 16-byte align
  };
  static_assert(sizeof(CloudMetaGPU) == 128, "CloudMetaGPU std430 layout");

  struct PrimitiveCloudBucketBuffers
  {
    QRhiBuffer* raw_splats{};        int64_t rawSplatsCap{};
    QRhiBuffer* cloud_meta{};        int64_t cloudMetaCap{};
    QRhiBuffer* cloud_id_lookup{};   int64_t cloudIdLookupCap{};
    QRhiBuffer* indirect{};          int64_t indirectCap{};
    uint32_t row_stride{};           // cached from cloud->row_stride
    uint64_t last_seen_frame{};      // for stale-bucket eviction
    // Per-frame content fingerprint over (per cloud in bucket order):
    //   raw_data identity + content_hash + primitive_count
    //   + worldTransform bytes + transform_slot
    // — i.e. everything the bucket's GPU buffers depend on. When the
    // computed fingerprint matches the stored one, the bucket's
    // raw_splats / cloud_meta / cloud_id_lookup / indirect buffers are
    // already correct from the previous frame and the per-frame CPU
    // concat + uploadStaticBuffer work can be skipped wholesale. 0 =
    // "never uploaded; force the first frame's upload regardless".
    // This is a delta-update step toward a fully persistent arena design.
    uint64_t content_fingerprint{};
  };
  ossia::flat_map<uint32_t, PrimitiveCloudBucketBuffers> m_primitiveCloudBuckets;
  uint64_t m_primitiveCloudFrame{0};

  // ─── Unified-MDI per-instance concat buffers ────────────────────────
  // Three parallel arrays sized to K = (Σ regular_cmd_count + Σ
  // instance_group_count). One slot per (cmd, instance) pair, contiguous
  // within a cmd. Each indirect cmd sets `firstInstance = its first
  // slot`, so per-instance VERTEX_INPUTs (translation / color / draw_id)
  // step at the right offset on both indirect and CPU-fallback paths
  // (firstInstance is honoured uniformly by every QRhi backend).
  //
  // - m_instTranslations: vec4-padded translation per slot (xyz used,
  //   w pad). Identity (0,0,0) for regular-mesh slots; actual
  //   per-particle position for instance-group slots (GPU-copied from
  //   the Instancer's source buffer with format-aware offsets).
  // - m_instColors: vec4 per slot. Identity (1,1,1,1) for regular-mesh
  //   slots; actual per-instance broadcast colour for groups.
  // - m_instDrawIds: uint per slot. Carries the cmd-index of the owning
  //   draw — replaces gl_DrawID (broken on CPU-fallback) and
  //   gl_BaseInstance (no longer = drawID once instanceCount > 1).
  QRhiBuffer* m_instTranslations{};
  QRhiBuffer* m_instColors{};
  QRhiBuffer* m_instDrawIds{};
  int64_t m_instTranslationsCap{};
  int64_t m_instColorsCap{};
  int64_t m_instDrawIdsCap{};
  uint32_t m_instSlotsUsed{};

  // CPU mirror of the draw_ids stream so we can diff-upload + cheaply
  // pre-fill identity values for regular cmds. Translations / colors
  // are GPU-resident sources for instance groups (no CPU mirror —
  // copies are GPU→GPU); we pre-fill identity for regular slots
  // straight into the GPU buffer via uploadStaticBuffer.
  std::vector<uint32_t> m_cachedInstDrawIds;

  // Prototype stable-id fallback map. Some producers (notably
  // Threedim::Primitive going through halp::geometry → legacy_geometry)
  // don't stamp a non-zero `mesh_primitive::stable_id` on their output.
  // Without a stable id, the slab arena allocates a fresh slab per
  // frame and the OffsetAllocator fragments until exhaustion. We cover
  // this by minting a stable id keyed on the prototype's
  // mesh_component pointer (which IS stable across frames as long as
  // the producer re-emits the same shared_ptr). GC pass at the end of
  // update() evicts entries whose pointer no longer appears in fs.
  ossia::hash_map<const ossia::mesh_component*, uint64_t> m_protoStableIds;

  // Pending GPU→GPU copy ops collected during update()'s accumulator loop
  // and executed in runInitialPasses (the only place ScenePreprocessor has a
  // live command buffer). Each op corresponds to one attribute of one
  // draw whose source buffer is GPU-resident; the CPU accumulator was
  // zero-filled in its place so all offsets stay consistent with the
  // tight MDI-layout contract. Cleared after being issued.
  enum class MdiAttr : uint8_t
  {
    Positions,
    Normals,
    Texcoords,
    Tangents
  };
  struct PendingGpuCopy
  {
    QRhiBuffer* src{};
    QRhiBuffer* dst{};   // explicit destination — when null, attr names
                         // a mesh-stream slot resolved via mdiBufferFor()
    int src_offset{};
    int dst_offset{};
    int size{};          // bytes if tight-copy, else element_size
    int vertex_count{};
    int src_stride{};    // 0 or element_size → tight; else strided
    int element_size{};  // BytesPerVertex for this attribute
    MdiAttr attr{};
  };
  std::vector<PendingGpuCopy> m_pendingGpuCopies;

  // Capacities (in bytes) of the two shared scene buffers — for growth-only.
  int64_t m_materialsExtCap{};

  // Per-channel material texture arrays are now owned by
  // GpuResourceRegistry and shared across all preprocessors in the same
  // RenderList. Sharing is safe because texture-source / layer
  // assignments are driven by asset identity (pointer to
  // texture_source), which is view-independent — every preprocessor
  // computes the same mapping. Shared arrays also let producers
  // (PBRMesh, MaterialOverride, loaders) author their own textureRefs
  // at update() time via the registry's resolve APIs without a
  // preprocessor-local dedup step.
  //
  // We stash the registry pointer at init() instead of going through
  // renderer.registry() at every call site — access is on the hot
  // rebuild path. Cleared on release(); m_lastRegistry below remembers
  // the previous pointer so the next init() can detect "same registry
  // as before release" and skip the cache wipe.
  GpuResourceRegistry* m_registry{};

  // Persist-across-rebuild contract: snapshot of m_registry at
  // release() time. Survives the release()/init() cycle so init() can
  // compare against the new RL's registry: equal → skip wipe (relink
  // graph, viewport resize when the renderer object is reused), unequal
  // → wipe (first init / OutputNode-replaced QRhi). Never read in the
  // hot path; only inspected from init().
  GpuResourceRegistry* m_lastRegistry{};

  // Convenience typedef + helper to localise the enum translation.
  using TexChannel = GpuResourceRegistry::TextureChannel;
  static TexChannel toTexChannel(MaterialChannel ch) noexcept
  {
    return static_cast<TexChannel>(ch);
  }
  auto& texChannel(MaterialChannel ch) noexcept
  {
    return m_registry->textureChannel(toTexChannel(ch));
  }
  const auto& texChannel(MaterialChannel ch) const noexcept
  {
    return m_registry->textureChannel(toTexChannel(ch));
  }

  // Uniform layer size — matching across channels keeps the samplers
  // interchangeable in shaders and simplifies sampler state.
  static constexpr int kChannelLayerSize
      = GpuResourceRegistry::kTextureLayerSize;

  // Content-based fingerprint of the materials list we last decoded. A
  // vector of raw material_component pointers (shared_ptr-element
  // identity). Stable across multi-producer scene merges: merge_scenes
  // concatenates material_component_ptr elements without deep-copying,
  // so the element pointers themselves don't change from frame to frame
  // even though the enclosing `shared_ptr<vector<...>>` does (the
  // _contributors > 1 branch in merge_scenes allocates a new vector
  // every merge). Comparing by content identity instead of the outer
  // pointer keeps the texture cache warm across multi-glTF scenes —
  // critical because re-decoding every JPEG and re-uploading every
  // 1024² layer every frame is the ~100ms/frame penalty we're fixing.
  std::vector<uint64_t> m_cachedMaterialsFingerprint;

  // -- Granular invalidation state ------------------------------------------
  //
  // We keep CPU mirrors of what's currently on the GPU for each small SSBO,
  // plus a fingerprint of the concatenated mesh list. Each frame we:
  //  * compare the fingerprint — if meshes unchanged, skip vertex/index
  //    upload entirely and keep m_outputSpec.meshes as the same shared_ptr
  //    (so downstream sees stable geometry_spec and doesn't rebuild any
  //    pipeline/SRB).
  //  * diff the mirror arrays against the freshly packed data and only
  //    uploadStaticBuffer(offset, size, …) for the contiguous ranges that
  //    actually changed. Moving a light thus costs one 64-byte partial
  //    upload; moving an object costs one PerDrawGPU (144 bytes).
  //
  // Memory cost: ~sizeof(T) × count on CPU (tens of KB for typical scenes).
  //
  // `m_cachedMeshFingerprint` stores `DrawCall::stable_id` per draw — the
  // address of the source mesh_primitive inside the stable mesh_component
  // shared_ptr (or the legacy ossia::geometry entry inside a mesh_list).
  // NOT `DrawCall::mesh`, because that points at a transient
  // primitiveToGeometry() wrapper that's freshly allocated on every
  // flattenScene() call and therefore changes every frame.
  std::vector<uint64_t> m_cachedMeshFingerprint;
  // Fingerprint of the primitive_cloud set. The fast path
  // (`meshesUnchanged`) skips rebuildPrimitiveClouds entirely — clouds are
  // NOT covered by m_cachedMeshFingerprint — so without this a cloud added
  // / removed / moved while the mesh fingerprint is unchanged would render
  // nothing / leave stale geometry / keep a stale CloudMetaGPU.model. Mixing
  // the cloud set into the fast-path gate forces the full rebuild branch
  // (which re-runs rebuildMDI + rebuildPrimitiveClouds) on any cloud change.
  // Covers the same fields rebuildPrimitiveClouds' internal per-bucket
  // fingerprint depends on (raw_data identity/content version, primitive
  // count, transform), plus the bucket key so add/remove is detected.
  uint64_t m_cachedCloudFingerprint{};
  // m_cachedMaterials is gone — scene_materials is the registry's
  // Material arena, not a preprocessor CPU mirror. Producers + the
  // loader-material upload pass write directly into arena slots.
  std::vector<MaterialExtensionsGPU> m_cachedMaterialExt;
  std::vector<PerDrawGPU> m_cachedPerDraws;
  // Mirror of the per_draw_bounds SSBO for diff-upload on the fast-path
  // (transforms/materials change but topology doesn't → tiny range
  // upload instead of full rewrite). Grow-only; same indexing as
  // m_cachedPerDraws.
  std::vector<PerDrawBoundsGPU> m_cachedPerDrawBounds;

  // Arena slots allocated by this preprocessor for loader materials
  // (materials entering scene_state.materials with raw_slot.size == 0,
  // i.e. not authored by a live producer like PBRMesh). The preprocessor
  // acts as a producer-on-behalf-of-loader for these: allocates one
  // Material arena slot per loader material, writes MaterialGPU bytes,
  // frees at release. Producer-authored materials already have their
  // own slots — those stay out of this map.
  ossia::hash_map<
      const ossia::material_component*, GpuResourceRegistry::Slot>
      m_loaderMaterialSlots;

  // Remembered accumulator sizes from the last full rebuildMDI. Used to
  // pre-reserve the temporary std::vector capacity so we don't pay for
  // repeated realloc + memmove when the scene grew or stays the same
  // size. Grow-only; never shrinks (negligible memory, big perf win for
  // scenes with many verts).
  // Vertex/index stream byte-sizes no longer tracked here — the
  // arena's OffsetAllocator owns sizing. `m_lastDrawCount` stays, used
  // to pre-reserve acc.perDraws / acc.indirectCmds.
  std::size_t m_lastDrawCount{};

  // Diff two CPU mirrors and partial-upload only the contiguous ranges
  // where fresh != cached. Also grows / shrinks the cached mirror to match
  // fresh's size. Returns true if at least one range was uploaded.
  //
  // When fresh.size() > cached.size() the new tail slots are appended +
  // uploaded. When fresh.size() < cached.size() the tail is zero-filled on
  // the GPU so stale content can't contribute (e.g. old lights with
  // intensity=1 still emitting after the scene shrank).
  template <typename T>
  static bool diffUpload(
      QRhiResourceUpdateBatch& res, QRhiBuffer* buf, std::vector<T>& cached,
      const std::vector<T>& fresh)
  {
    if(!buf)
      return false;
    bool changed = false;

    const std::size_t common = std::min(cached.size(), fresh.size());
    for(std::size_t i = 0; i < common;)
    {
      // Skip equal runs.
      if(std::memcmp(&cached[i], &fresh[i], sizeof(T)) == 0)
      {
        ++i;
        continue;
      }
      // Coalesce contiguous differing slots into one upload.
      std::size_t start = i;
      while(i < common
            && std::memcmp(&cached[i], &fresh[i], sizeof(T)) != 0)
      {
        cached[i] = fresh[i];
        ++i;
      }
      res.uploadStaticBuffer(
          buf, quint32(start * sizeof(T)),
          quint32((i - start) * sizeof(T)),
          reinterpret_cast<const char*>(&fresh[start]));
      changed = true;
    }

    if(fresh.size() > cached.size())
    {
      const std::size_t start = cached.size();
      cached.insert(cached.end(), fresh.begin() + start, fresh.end());
      res.uploadStaticBuffer(
          buf, quint32(start * sizeof(T)),
          quint32((fresh.size() - start) * sizeof(T)),
          reinterpret_cast<const char*>(&fresh[start]));
      changed = true;
    }
    else if(fresh.size() < cached.size())
    {
      // Zero the stale tail on GPU so shaders iterating the buffer's
      // capacity don't see ghost entries.
      std::vector<T> zeros(cached.size() - fresh.size());
      res.uploadStaticBuffer(
          buf, quint32(fresh.size() * sizeof(T)),
          quint32(zeros.size() * sizeof(T)),
          reinterpret_cast<const char*>(zeros.data()));
      cached.resize(fresh.size());
      changed = true;
    }
    return changed;
  }

  // Last-published geometry_spec; kept alive so downstream shared_ptr equality
  // sees stable identity across frames when the scene is unchanged.
  ossia::geometry_spec m_outputSpec;

  // Cache: identity of last input scene (raw scene_state* pointer + version).
  const ossia::scene_state* m_cachedSceneState{};
  int64_t m_cachedVersion{-1};

  RenderedScenePreprocessorNode(const ScenePreprocessorNode& n)
      : NodeRenderer{n}
      , m_node{n}
  {
  }

  // The incremental-reconciliation path (Graph::incrementalEdgeUpdate)
  // creates fresh renderers and calls `initState()` on them, NOT `init()`.
  // Our preprocessor has no per-edge state — everything lives at the
  // init() level — so both entry points run the same setup. Without
  // this delegation a preprocessor created via the incremental path
  // never has `m_registry` set, every `rebuildChannel` call early-outs,
  // and consumer shaders see empty texture arrays (the exact
  // "textures gone on second play" failure mode observed on stop/start).
  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    init(renderer, res);
  }

  void releaseState(RenderList& renderer) override
  {
    release(renderer);
  }

  // Reset every per-RenderList / per-registry cache field to empty.
  // Frees registry-allocated slots (loader-material, env) when
  // `freeRegistryResources` is true — pass true from release() (we
  // still hold a valid m_registry) and false from init() (the prior
  // m_registry, if any, may already be torn down: we cannot legally
  // free against it; just drop the bookkeeping so arenaSlotForMaterial
  // and the env publish path don't reuse stale slot indices on the
  // fresh registry).
  //
  // QRhiBuffer-backed fields (m_materialsExtBuffer, m_lightIndicesBuffer,
  // m_camerasBuffer, m_mdi.*, m_inst*, m_skinBuffers, m_sceneDataBuffers,
  // m_sceneCountsBuffer, m_shadowCascadesBuffer, m_worldTransforms*Buffer)
  // and their paired *Cap counters are NOT touched here — they go
  // through dropBuf / renderer.releaseBuffer in release() because they
  // need the renderer's release plumbing.
  void clearAllCaches(bool freeRegistryResources, uint32_t current_frame = 0u)
  {
    if(freeRegistryResources && m_registry)
    {
      for(auto& [mat, slot] : m_loaderMaterialSlots)
        if(slot.valid())
          m_registry->free(slot);
      if(m_envSlot.valid())
        m_registry->free(m_envSlot);
      // MeshSlab leak fix: every (mc, id) pair in m_protoStableIds is a
      // stable_id WE minted (resolvePrototypeStableId line 1377). The
      // matching slab is in the registry's m_meshSlabs cache. Clearing
      // m_protoStableIds without releasing the slabs leaves them as
      // orphans: the next renderer instance mints DIFFERENT IDs (mints
      // are globally unique), so its acquireMeshSlab calls miss the
      // cache and allocate fresh slabs. sweepMeshSlabs ages out the
      // orphans after `grace=2` frames -- but rapid drag-resize
      // triggers another rebuild before grace elapses, so slabs
      // accumulate (used grew 70074 → 420444 in 6 resizes for the
      // user's repro). Release explicitly here so the next-frame
      // sweep can immediately reclaim. Routes through grace queue so
      // any in-flight CB still referencing the slab is safe.
      for(auto& [mc, id] : m_protoStableIds)
        if(id != 0)
          m_registry->releaseMeshSlab(id, current_frame);
    }
    m_loaderMaterialSlots.clear();
    m_envSlot = {};
    m_envSlotSeeded = false;
    m_protoStableIds.clear();

    m_cachedSceneState = nullptr;
    m_cachedVersion = -1;
    m_cachedMaterialsFingerprint.clear();
    m_cachedMeshFingerprint.clear();
    m_cachedCloudFingerprint = 0;
    m_cachedMaterialExt.clear();
    m_cachedPerDraws.clear();
    m_cachedPerDrawBounds.clear();
    m_cachedShadowCascades = {};
    m_shadowCascadesSeeded = false;
    m_cachedSceneCounts = {~0u, ~0u, ~0u, 0u};
    m_cachedMaterialUVTransforms.clear();
    m_cachedCameras.clear();
    m_lastCameraUploadFrame = -1;
    m_cachedInstDrawIds.clear();
    m_cachedLightIndices.clear();
    m_lastEnvUpload = {};
    m_outputSpec = {};
    m_lastDrawCount = 0;
  }

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    m_initialized = true;

    // Persist-across-rebuild contract: if the OutputNode-owned registry
    // is the SAME pointer we held in the previous init() / release()
    // cycle, every slot index (m_loaderMaterialSlots, m_envSlot, ...)
    // and the texture-array channels are still alive — re-allocating
    // them on a viewport resize / relink would re-upload ~100 MiB of
    // decoded textures and pay the 50–500 ms rebuild burst this whole
    // refactor exists to avoid.
    //
    // Skip the cache wipe in that case. The fingerprint / per-draw /
    // cascade caches will naturally match the unchanged scene state on
    // the first post-rebuild frame, short-circuiting the REBUILD branch
    // (see needsRebuild gate around line 4051) and rebuildChannel's
    // sameMaterialsContent fast path → no texture re-upload.
    //
    // The pre-release pointer is stashed in m_lastRegistry; m_registry
    // itself is null between release() and init() (so that any stray
    // post-release rebuildChannel call hits its guarded early-out
    // instead of dereferencing a stale pointer). m_lastRegistry == null
    // means "first ever init on this renderer" → wipe (no-op since
    // there's nothing to wipe). m_lastRegistry != new_registry means
    // the OutputNode tore its registry down and built a fresh one
    // (setSwapchainFormat / QRhi-replacement) → wipe (any slot indices
    // we held are stale).
    auto* new_registry = &renderer.registry();
    const bool registry_changed = (m_lastRegistry != new_registry);
    if(registry_changed)
    {
      // Drop every per-registry cache before swapping m_registry. If a
      // previous RenderList left state behind (incremental edge rebuild
      // without an intervening release()), m_loaderMaterialSlots /
      // m_envSlot / m_protoStableIds carry slot indices that the new
      // registry never allocated — arenaSlotForMaterial would silently
      // return them and every mesh would wear the wrong material. The
      // fingerprint / per-draw / cascade caches likewise gate dirty
      // detection against the prior scene state. We can't legally free
      // against the old registry (it may already be torn down), so we
      // pass freeRegistryResources=false: just drop the bookkeeping.
      clearAllCaches(/*freeRegistryResources=*/false);
    }
    // else: registry survived (resize fast path / relinkGraph reuse).
    // Keep m_loaderMaterialSlots / m_envSlot / fingerprints / per-draw
    // caches — they all reference live state in the persistent registry.
    m_registry = new_registry;
    m_lastRegistry = new_registry;

    // Claim our own Env arena slot for the merged environment upload.
    // Each preprocessor owns a slot — needed because two
    // preprocessors can receive different filtered views of the same
    // source scene and must not stomp each other's merged env.
    if(!m_envSlot.valid())
    {
      m_envSlot = m_registry->allocate(
          GpuResourceRegistry::Arena::Env, sizeof(EnvParamsUBO));
      m_envSlotSeeded = false;
    }

    // Pre-allocate a 1-layer BaseColor array with a white fallback so
    // downstream consumers (classic_pbr_textured) building their samplers
    // in their own init() get a real texture pointer via textureForOutput,
    // not nullptr. update() will reallocate with the right layer count
    // once the scene is flattened. First preprocessor to run init() does
    // this; subsequent preprocessors see the array already allocated and
    // skip (shared registry state).
    auto& rhi = *renderer.state.rhi;
    auto& bc = texChannel(ChannelBaseColor);
    if(!bc.primaryArray())
    {
      auto& b = bc.ensurePrimary(
          QRhiTexture::RGBA8,
          QSize(kChannelLayerSize, kChannelLayerSize));
      b.array = rhi.newTextureArray(
          b.format, 1, b.pixelSize, 1,
          GpuResourceRegistry::textureChannelFlags(toTexChannel(ChannelBaseColor)));
      if(b.array)
      {
        b.array->setName("GpuResourceRegistry::base_color_array (init fallback)");
        if(!b.array->create())
        {
          delete b.array;
          b.array = nullptr;
        }
      }
      if(b.array)
      {
        b.layers = 1;
        QImage w(1, 1, QImage::Format_RGBA8888);
        w.fill(Qt::white);
        w = w.scaled(
            kChannelLayerSize, kChannelLayerSize,
            Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QRhiTextureSubresourceUploadDescription sub(w);
        QRhiTextureUploadEntry entry(0, 0, sub);
        res.uploadTexture(
            b.array, QRhiTextureUploadDescription({entry}));
      }
      else
      {
        // Allocation failed — drop the empty bucket so primaryArray()
        // stays null and callers hit the "no array" fallback path.
        bc.buckets.clear();
      }
    }
  }

  void release(RenderList& renderer) override
  {
    // QRhiBuffer invariant: go through RenderList::releaseBuffer so any
    // buffer still referenced by a downstream mesh's MeshBuffers skips
    // deleteLater (the mesh iteration at RenderList::release will
    // destroy it via `delete b.handle`). Bypassing releaseBuffer with
    // `deleteLater` directly is what caused the "rare segfault on exit"
    // — the same pointer ending up in the final `delete b.handle` pass.
    auto dropBuf = [&](QRhiBuffer*& b) {
      if(b) { renderer.releaseBuffer(b); b = nullptr; }
    };
    dropBuf(m_lightIndicesBuffer);
    // m_materialsBuffer + m_lightsBuffer removed — scene_materials and
    // scene_lights bind the registry arenas directly.
    dropBuf(m_materialsExtBuffer);
    dropBuf(m_materialUVTransformsBuffer);
    m_materialUVTransformsCap = 0;
    for(auto& sd : m_sceneDataBuffers)
      if(sd.owned && sd.buffer) renderer.releaseBuffer(sd.buffer);
    m_sceneDataBuffers.clear();
    for(auto& sk : m_skinBuffers)
      if(sk.buffer) renderer.releaseBuffer(sk.buffer);
    m_skinBuffers.clear();
    // Vertex/index streams are registry-owned; only the
    // preprocessor-owned per_draws + indirect_draw_cmds + per_draw_bounds
    // drop here.
    dropBuf(m_mdi.per_draws);
    dropBuf(m_mdi.indirect_draw_cmds);
    dropBuf(m_mdi.per_draw_bounds);
    m_mdi = {};
    // Per-bucket primitive cloud resources.
    for(auto& [k, bb] : m_primitiveCloudBuckets)
    {
      dropBuf(bb.raw_splats);
      dropBuf(bb.cloud_meta);
      dropBuf(bb.cloud_id_lookup);
      dropBuf(bb.indirect);
    }
    m_primitiveCloudBuckets.clear();
    dropBuf(m_instTranslations);
    dropBuf(m_instColors);
    dropBuf(m_instDrawIds);
    m_instTranslationsCap = 0;
    m_instColorsCap = 0;
    m_instDrawIdsCap = 0;
    m_instSlotsUsed = 0;
    m_lightIndicesCap = 0;
    m_materialsExtCap = 0;
    // Texture channel arrays are owned by GpuResourceRegistry — no
    // per-preprocessor cleanup needed. They get destroyed when the
    // RenderList tears down (registry.destroy()).
    dropBuf(m_sceneCountsBuffer);
    dropBuf(m_shadowCascadesBuffer);
    dropBuf(m_camerasBuffer);
    dropBuf(m_camerasPrevBuffer);
    m_camerasCap = 0;
    dropBuf(m_worldTransformsBuffer);
    dropBuf(m_worldTransformsPrevBuffer);
    m_worldTransformsCap = 0;
    m_pendingWorldXformWrites.clear();
    m_pendingWorldXformWrites.shrink_to_fit();
    m_lastSnapshotFrame = -1;
    // Symmetric clear for m_pendingGpuCopies: ops record raw QRhiBuffer*
    // for src/dst (m_mdi.* and m_primitiveCloudBuckets buffers) which
    // dropBuf above just released. Today release() is followed by either
    // node teardown (no further runInitialPasses) or init() + a new
    // rebuildMDI which clears the queue at its top, so the dangling
    // pointers are never dereferenced — but the asymmetry is fragile
    // against any future reordering. Defensive.
    m_pendingGpuCopies.clear();
    m_pendingGpuCopies.shrink_to_fit();
    m_lastGpuCopiesFrame = -1;
    // Env arena buffer is owned by GpuResourceRegistry — nothing to drop here.
    // Stream byte-size trackers removed (see m_mdi comment).

    // Free per-registry resources on every release(), regardless of
    // whether the renderer will be destroyed (recreateOutputRenderList)
    // or reused (relinkGraph). The "skip wipe on registry-pointer
    // match" optimization the previous version of this comment
    // referenced ONLY benefits the relinkGraph path; on resize the
    // renderer is freshly constructed so m_loaderMaterialSlots etc.
    // are already empty.
    //
    // The bug it caused: m_envSlot was leaked on every release().
    // The Env arena has only 8 slots (GpuResourceRegistry.cpp:69), so
    // after 8 resizes the arena exhausted, m_envSlot allocation fell
    // back to slot 0 (or invalid), and the env aux binding pointed at
    // slot 0's stale data — wildly wrong lighting / fog / exposure
    // that drifts each resize as different stale data lands at slot 0.
    // Other arenas have more headroom (Material 32K, RawTransform
    // 16K) but they still leak; over many resizes the same drift
    // would surface there.
    //
    // Trade-off: relinkGraph now pays the cost of re-allocating the
    // env slot + per-loader-material slots + clearing the texture
    // fingerprint (~10s of ms). Acceptable — relinkGraph is rare
    // (user changes graph); resize is common (drag-resize fires
    // continuously).
    clearAllCaches(/*freeRegistryResources=*/true, (uint32_t)renderer.frame);

    // Clear the registry pointer so a post-release rebuildChannel call
    // hits its guarded early-out rather than dereferencing the
    // pre-release pointer. m_lastRegistry stays populated for any
    // future re-init wanting to detect "same registry as before".
    m_lastRegistry = m_registry;
    m_registry = nullptr;
    m_initialized = false;
  }

  // Source byte size of one element of an ossia::geometry attribute format.
  // Used to bound CPU attribute reads so an attribute authored in a smaller
  // format than the consumer expects (e.g. an unorm-byte4 color, 4 B,
  // read as float4, 16 B) doesn't over-read the source buffer.
  static int geomAttrFormatByteSize(int format) noexcept
  {
    using A = ossia::geometry::attribute;
    switch(format)
    {
      case A::float4:                            return 16;
      case A::float3:                            return 12;
      case A::float2:                            return 8;
      case A::float1:                            return 4;
      case A::unormbyte4:                        return 4;
      case A::unormbyte2:                        return 2;
      case A::unormbyte1:                        return 1;
      case A::uint4: case A::sint4:              return 16;
      case A::uint3: case A::sint3:              return 12;
      case A::uint2: case A::sint2:              return 8;
      case A::uint1: case A::sint1:              return 4;
      case A::half4:                             return 8;
      case A::half3:                             return 6;
      case A::half2:                             return 4;
      case A::half1:                             return 2;
      case A::ushort4: case A::sshort4:          return 8;
      case A::ushort3: case A::sshort3:          return 6;
      case A::ushort2: case A::sshort2:          return 4;
      case A::ushort1: case A::sshort1:          return 2;
      default:                                   return 0; // user_struct / unknown
    }
  }

  // Read a single vertex attribute's full range from a CPU-backed source
  // geometry into a freshly-allocated contiguous byte buffer. Returns empty
  // if the source uses a GPU handle, is missing, or has an unsupported
  // format. `BytesPerVertex` is the consumer's expected element size.
  template <int BytesPerVertex>
  static std::vector<std::byte> extractCpuAttribute(
      const ossia::geometry& g, ossia::attribute_semantic sem)
  {
    const auto* a = g.find(sem);
    if(!a)
      return {};
    if(a->binding < 0 || a->binding >= (int)g.input.size())
      return {};
    const auto& in = g.input[a->binding];
    if(in.buffer < 0 || in.buffer >= (int)g.buffers.size())
      return {};
    const auto& b = g.buffers[in.buffer];
    const auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&b.data);
    if(!cpu || !cpu->raw_data)
      return {};

    const int stride = (a->binding < (int)g.bindings.size())
        ? (int)g.bindings[a->binding].byte_stride
        : BytesPerVertex;

    // Copy at most the source element's byte size into the destination
    // element (the rest stays zero-filled). An attribute whose source
    // format is narrower than BytesPerVertex (e.g. unorm-byte4 color, 4 B,
    // consumed as float4, 16 B) must not pull 12 stray bytes per vertex.
    const int srcElem = geomAttrFormatByteSize(a->format);
    const int copyPerVertex
        = (srcElem > 0) ? std::min(BytesPerVertex, srcElem) : BytesPerVertex;

    // Bound every read against the source buffer's actual byte_size:
    // an inconsistent producer (short buffer, wrong vertex_count) must not
    // over-read off the end of the heap allocation.
    const int64_t baseOff = (int64_t)in.byte_offset + (int64_t)a->byte_offset;
    const int64_t srcBytes = cpu->byte_size;
    if(baseOff < 0 || (srcBytes > 0 && baseOff >= srcBytes))
      return {};

    std::vector<std::byte> out(std::size_t(g.vertices) * BytesPerVertex);
    const auto* raw = reinterpret_cast<const std::byte*>(cpu->raw_data.get());
    const auto* base = raw + baseOff;
    for(int i = 0; i < g.vertices; ++i)
    {
      const int64_t off = baseOff + (int64_t)i * stride;
      // Clamp this element's copy so it never reads past byte_size.
      int n = copyPerVertex;
      if(srcBytes > 0)
      {
        const int64_t avail = srcBytes - off;
        if(avail <= 0)
          break; // remaining vertices stay zero-filled
        if(avail < n)
          n = (int)avail;
      }
      std::memcpy(out.data() + std::size_t(i) * BytesPerVertex,
                  base + (int64_t)i * stride, n);
    }
    return out;
  }

  // GPU-backed counterpart of extractCpuAttribute. Returns the backing
  // QRhiBuffer* + source byte offset + stride for the requested semantic
  // when the mesh's buffer is a gpu_buffer variant (upstream compute
  // shader output, etc). Empty when the attribute is missing or the
  // buffer is CPU-resident.
  struct GpuAttrView
  {
    QRhiBuffer* buf{};
    int src_offset{};
    int byte_stride{};
  };
  static GpuAttrView
  extractGpuAttribute(const ossia::geometry& g, ossia::attribute_semantic sem)
  {
    const auto* a = g.find(sem);
    if(!a)
      return {};
    if(a->binding < 0 || a->binding >= (int)g.input.size())
      return {};
    const auto& in = g.input[a->binding];
    if(in.buffer < 0 || in.buffer >= (int)g.buffers.size())
      return {};
    const auto& b = g.buffers[in.buffer];
    const auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&b.data);
    if(!gpu || !gpu->handle)
      return {};
    GpuAttrView v;
    v.buf = static_cast<QRhiBuffer*>(gpu->handle);
    v.src_offset = int(in.byte_offset + a->byte_offset);
    v.byte_stride = (a->binding < (int)g.bindings.size())
                        ? (int)g.bindings[a->binding].byte_stride
                        : 0;
    return v;
  }

  static std::vector<uint32_t> extractCpuIndices(const ossia::geometry& g)
  {
    if(g.index.buffer < 0 || g.index.buffer >= (int)g.buffers.size())
      return {};
    const auto& b = g.buffers[g.index.buffer];
    const auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&b.data);
    if(!cpu || !cpu->raw_data)
      return {};

    // Bound the index read against the source byte_size: a
    // short / inconsistent index buffer must not over-read the heap. Clamp
    // the readable index count to what fits past byte_offset.
    const int idxBytes
        = (g.index.format == decltype(g.index)::uint16) ? 2 : 4;
    const int64_t baseOff = (int64_t)g.index.byte_offset;
    const int64_t srcBytes = cpu->byte_size;
    if(baseOff < 0 || (srcBytes > 0 && baseOff >= srcBytes))
      return {};
    int readable = g.indices;
    if(srcBytes > 0)
    {
      const int64_t avail = (srcBytes - baseOff) / idxBytes;
      if(avail < readable)
        readable = (int)std::max<int64_t>(avail, 0);
    }

    std::vector<uint32_t> out(g.indices); // tail (if clamped) stays 0
    const auto* base = reinterpret_cast<const std::byte*>(cpu->raw_data.get())
                       + baseOff;
    if(g.index.format == decltype(g.index)::uint16)
    {
      const auto* src = reinterpret_cast<const uint16_t*>(base);
      for(int i = 0; i < readable; ++i)
        out[i] = src[i];
    }
    else
    {
      std::memcpy(out.data(), base, std::size_t(readable) * 4);
    }
    return out;
  }

  // Mesh-deterministic subset of emitDraw's skip predicate.
  // emitDraw drops a draw when:
  //   (a) the mesh has no usable positions (neither CPU nor GPU sourced), or
  //   (b) it has indices but they're GPU-backed (extractCpuIndices empty).
  // Both depend only on the mesh's buffers, which are invariant while the
  // mesh fingerprint matches — so the fast path can replicate them here to
  // keep its freshPerDraws mirror in lock-step with what emitDraw packed.
  // The remaining emitDraw skips (null mesh / vertices<=0 / null registry /
  // slab exhaustion) are handled at the fast-path call site or cannot occur
  // once a slab is already resident.
  static bool meshEmitsDraw(const ossia::geometry& mesh)
  {
    const bool hasCpuPos
        = !extractCpuAttribute<12>(mesh, ossia::attribute_semantic::position)
               .empty();
    if(!hasCpuPos)
    {
      const auto gpu_pos
          = extractGpuAttribute(mesh, ossia::attribute_semantic::position);
      if(!gpu_pos.buf)
        return false; // no positions → emitDraw skips
    }
    if(mesh.indices > 0 && extractCpuIndices(mesh).empty())
      return false; // GPU-backed indices unsupported → emitDraw skips
    return true;
  }

  // Grow-only allocate / reuse a single QRhiBuffer.
  //
  // Releases the old handle via RenderList::releaseBuffer — which is the
  // project-wide invariant for QRhiBuffer lifetime: releaseBuffer scans
  // the RenderList's m_vertexBuffers for the pointer and either skips
  // (when the buffer is still referenced by a mesh, so the mesh iteration
  // at RenderList::release will clean it up) or deleteLater's (when it
  // isn't referenced). Calling QRhiBuffer::deleteLater directly bypasses
  // that check and causes a double-free on RenderList::release for any
  // buffer that was also stored in a MeshBuffers entry — the "sometimes
  // segfault on exit" crash pattern.
  // Returns true when the buffer was (re)allocated this call. Callers
  // pairing the buffer with a diffUpload-managed CPU mirror MUST clear
  // that mirror on `true` so diffUpload re-emits the full fresh
  // contents into the new (uninitialised) allocation. Without this,
  // diffUpload's equal-prefix short-circuit (lines 779-801) leaves the
  // freshly-allocated GPU buffer's prefix bytes uninitialised whenever
  // the new fresh values match the previous frame's cached values
  // (e.g. an Instancer with one prototype emits draw_id=0 for every
  // slot — every cross of the power-of-two capacity boundary leaks the
  // first cached.size() entries as driver-uninit memory). Manifests as
  // "instances disappear at counts 4→5 / 8→9 / 16→17 / …" because the
  // prototype's vertex shader reads garbage draw_id and OOBs on
  // per_draws[draw_id].
  static bool growBuf(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      QRhiBuffer*& buf, int64_t& cap,
      int64_t need, QRhiBuffer::UsageFlags flags, const char* name)
  {
    if(buf && cap >= need)
      return false;
    // Capacity policy: pure power-of-two doubling overshoots badly for
    // large buffers (a 1.08 GB request landed on a 2 GB allocation, which
    // QRhi/Vulkan/D3D commonly reject around the 2³¹ byte boundary —
    // many driver paths cap maxStorageBufferRange at 2GB-4 or use a
    // signed-int32 size internally). Switch policy at a 256 MB knee:
    // small buffers double (so frequent grows don't thrash); huge
    // buffers grow by 25 % over need (still amortised, but never
    // doubles past a 2 GB cliff for a sub-2 GB need). Aligned to 16 B
    // so std430 structures land on natural strides.
    constexpr int64_t kKnee = 256ll * 1024 * 1024; // 256 MB
    int64_t newCap = cap > 0 ? cap : 16;
    while(newCap < need)
    {
      if(newCap < kKnee)
        newCap *= 2;
      else
        newCap = (need * 5 / 4 + 15) & ~int64_t{15};
    }
    auto* old = buf;
    if(buf)
      renderer.releaseBuffer(buf);
    buf = renderer.state.rhi->newBuffer(QRhiBuffer::Static, flags, newCap);
    buf->setName(name);
    // QRhi::create() returns false on driver-level allocation failure
    // (out of VRAM, exceeds maxBufferSize, signed-32-bit overflow in
    // the backend). Without this check we'd publish a zombie wrapper
    // whose underlying VkBuffer/D3D buffer is null; uploadStaticBuffer
    // becomes a silent no-op and the GPU sees zero-filled memory at
    // every read. That's exactly the "all splats collapse to origin"
    // signature in the 3DGS pipeline. Surface the failure loudly.
    const bool ok = buf->create();
    BUFTRACE() << "ScenePreprocessor::growBuf name=" << name
               << " old=" << (void*)old
               << " new=" << (void*)buf
               << " cap=" << (qint64)cap << "->" << (qint64)newCap
               << " need=" << (qint64)need
               << " ok=" << ok;
    if(!ok)
    {
      qWarning() << "ScenePreprocessor::growBuf:" << name
                 << "create() FAILED at cap=" << (qint64)newCap
                 << "(need=" << (qint64)need
                 << "). Driver likely refused the allocation —"
                    " too large, OOM, or hit a backend size limit."
                    " Downstream reads will return zeros.";
    }
    else
    {
      // Zero-fill the freshly allocated buffer. Vulkan does NOT
      // zero-initialise new VkBuffers — the underlying device-memory
      // page contains whatever was there before. For sparse-uploaded
      // SSBOs (per_draws padding past drawCount, world_transforms
      // unused arena slots, etc.) the un-touched bytes would otherwise
      // be read by shaders (especially when an indexer like
      // PerDraw.transform_slot points at a slot the producer hasn't
      // populated this frame) and feed garbage into the pipeline.
      // After resize, each fresh VkBuffer gets a different page →
      // wildly different visual results per resize. RhiClearBuffer
      // pulls the zero source bytes from a thread-local pool — no
      // per-call std::vector<char>(newCap, 0) allocation.
      RhiClearBuffer::clearBuffer(
          *renderer.state.rhi, res, buf, 0, (quint32)newCap);
    }
    cap = newCap;
    return true;
  }

  // Resolve a material_component pointer to its Material-arena slot
  // index. Producer-authored materials carry a live raw_slot; loader
  // materials get one allocated in m_loaderMaterialSlots. Returns 0
  // when no slot is found — matches an unused arena entry, so shaders
  // fall back to a default-initialised MaterialGPU rather than reading
  // undefined bytes.
  //
  // This is the value stamped into
  // `PerDrawGPU.material_index`, NOT the scene.state->materials index.
  // Both the fast-path per_draws pack (update()) and the full-rebuild
  // pack (rebuildMDI) must use this helper so the arena slot index is
  // consistent across meshes-changed and meshes-unchanged paths.
  uint32_t arenaSlotForMaterial(const ossia::material_component* mat) const noexcept
  {
    if(!mat || !m_registry)
      return 0u;
    if(m_registry->isLive(mat->raw_slot))
      return mat->raw_slot.internal_index;
    auto it = m_loaderMaterialSlots.find(mat);
    if(it != m_loaderMaterialSlots.end() && it->second.valid())
      return it->second.slot_index;
    return 0u;
  }

  // Resolve a stable id for an instance prototype. Producers SHOULD stamp
  // mesh_primitive::stable_id at construction (loaders do, PBRMesh does);
  // when they don't (notably Threedim::Primitive routed through
  // halp::geometry → mesh_component::legacy_geometry, which carries no
  // primitive list at all and is bridged into a synthesized primitive
  // upstream), we mint our own id keyed on the mesh_component pointer
  // — stable across frames as long as the producer re-emits the same
  // shared_ptr, which the identity-caching pattern enforces.
  uint64_t resolvePrototypeStableId(
      const ossia::mesh_component* mc,
      const ossia::mesh_primitive& prim) noexcept
  {
    if(prim.stable_id != 0)
      return prim.stable_id;
    if(!mc)
      return reinterpret_cast<uint64_t>(&prim);
    auto [it, inserted] = m_protoStableIds.emplace(mc, 0u);
    if(inserted)
      it->second = ossia::mint_stable_id();
    return it->second;
  }

  // MDI rebuild: concatenate CPU-backed legacy_geometry meshes into shared
  // vertex / index buffers + emit one output geometry with indirect draw
  // metadata. Draws whose source is GPU-backed or uses non-standard formats
  // are skipped with a warning (they can be rendered through per-mesh mode).
  //
  // The MeshArenaManager's slab lifecycle is exercised here —
  // `acquireMeshSlab` + `markMeshSlabSeen` per-draw, `sweepMeshSlabs` at
  // the end. Slabs are allocated and their offsets are available, but
  // the concat-and-bulk-upload path below still runs unchanged, so
  // rendering stays byte-identical to before slab tracking was added.
  //
  // TODO: replace `uploadStaticBuffer` at
  // offset 0 over concatenated ACC vectors with per-slab
  // `registry.uploadMeshStream(slab, Stream, bytes, size)` calls, gated
  // by `slab->freshly_allocated`. Output geometry's vertex/index buffer
  // bindings switch from `m_mdi.positions` to
  // `registry.meshStreamBuffer(MeshStream::Positions)`. indirect_draw_cmds
  // entries take their `baseVertex` / `firstIndex` from the slab's
  // stream offsets. GPU-to-GPU copies (m_pendingGpuCopies) point at
  // slab offsets too. Net effect: adding one mesh uploads only that
  // mesh's bytes; no scene-wide reconcat.
  // Primitive-cloud branch — buckets fs.primitive_clouds by format_id
  // and emits one indirect-draw geometry per bucket. Each bucket
  // geometry is appended to m_outputSpec.meshes after the mesh MDI
  // entry (if any). Per bucket emits:
  //   - one auxiliary SSBO `raw_splats` (concatenation of cloud
  //     raw_data buffers; same row stride across the bucket's clouds)
  //   - one auxiliary SSBO `cloud_meta` (CloudMetaGPU[] mirroring
  //     PerDrawGPU's model[16] + transform_slot pattern)
  //   - one auxiliary SSBO `cloud_id_lookup` (uint per primitive ->
  //     index into cloud_meta)
  //   - one indirect cmd buffer {vertex_count=6, instance_count=Σ
  //     primitive_counts, ...} so RawRaster's existing m_mesh->draw()
  //     path picks up the draw via cb.drawIndirect or the cpu_draw
  //     fallback.
  //
  // The format's first CSF stage reads `raw_splats` via AUXILIARY
  // LAYOUT (no per-column SSBO bindings, so descriptor budget stays
  // tight on integrated Metal).
  void rebuildPrimitiveClouds(
      RenderList& renderer, QRhiResourceUpdateBatch& res,
      const FlatScene& fs)
  {
    ++m_primitiveCloudFrame;
    if(fs.primitive_clouds.empty())
    {
      // No clouds this frame — keep buckets around for one frame in
      // case the scene briefly goes empty during a graph rebuild, but
      // the persistent buffers are released by releaseBuffer() when
      // the renderer torn down. Stale eviction only fires when the
      // primitive_clouds list is non-empty (below).
      return;
    }

    // Bucket the entries. flat_map<bucket_key, vector<entry index>>.
    // bucket_key was already chosen by the visitor: hash(format_id) or
    // stable_id when format_id is empty (each unformatted cloud
    // becomes its own bucket).
    struct Bucket
    {
      uint32_t bucket_key;
      ossia::small_vector<const FlatScene::PrimitiveCloudDraw*, 4> draws;
      uint64_t total_primitives{};
      uint32_t row_stride{};
      int64_t  raw_splats_bytes{};
    };
    ossia::flat_map<uint32_t, Bucket> buckets;

    for(const auto& d : fs.primitive_clouds)
    {
      if(!d.cloud || d.cloud->primitive_count == 0)
        continue;
      // Bucket by format_id when set, else by cloud's address (stable
      // pointer keyed bucket). Mirrors the visitor's intent. Hash matches
      // the canonical filter_tag stamp (ossia::hash_string truncated to
      // 32 bits) so a downstream FlattenedSceneFilterNode "format_id ==
      // match_str" route lines up byte-for-byte with this bucket key.
      uint32_t key = 0;
      if(!d.cloud->format_id.empty())
      {
        key = (uint32_t)ossia::hash_string(d.cloud->format_id);
      }
      else
      {
        key = (uint32_t)((uintptr_t)d.cloud.get() & 0xffffffffu);
      }

      auto& b = buckets[key];
      if(b.draws.empty())
      {
        b.bucket_key = key;
        b.row_stride = d.cloud->row_stride;
      }
      else if(b.row_stride != d.cloud->row_stride)
      {
        // Row-stride mismatch in a same-key bucket: skip the
        // mismatched cloud rather than corrupt the concat. Indicates
        // a tagging error in the producer.
        qWarning() << "ScenePreprocessor::rebuildPrimitiveClouds: "
                      "row_stride mismatch within bucket"
                   << QString::fromStdString(d.cloud->format_id)
                   << " expected" << b.row_stride
                   << "got" << d.cloud->row_stride;
        continue;
      }
      b.draws.push_back(&d);
      b.total_primitives += d.cloud->primitive_count;
    }

    // Drop buckets whose key did not appear this frame.
    for(auto it = m_primitiveCloudBuckets.begin();
        it != m_primitiveCloudBuckets.end();)
    {
      if(buckets.find(it->first) == buckets.end())
      {
        auto& bb = it->second;
        if(bb.raw_splats)       renderer.releaseBuffer(bb.raw_splats);
        if(bb.cloud_meta)       renderer.releaseBuffer(bb.cloud_meta);
        if(bb.cloud_id_lookup)  renderer.releaseBuffer(bb.cloud_id_lookup);
        if(bb.indirect)         renderer.releaseBuffer(bb.indirect);
        it = m_primitiveCloudBuckets.erase(it);
      }
      else
      {
        ++it;
      }
    }

    using UF = QRhiBuffer::UsageFlags;

    // Lazily ensure m_outputSpec.meshes exists so we can append.
    if(!m_outputSpec.meshes)
      m_outputSpec.meshes = std::make_shared<ossia::mesh_list>();
    if(!m_outputSpec.filters)
      m_outputSpec.filters = std::make_shared<ossia::geometry_filter_list>();

    // Cow if shared with downstream — the mesh MDI rebuilds via
    // make_shared<mesh_list>() so the typical state is non-shared
    // here. If a downstream reader is holding the previous list, we
    // need a fresh one to avoid mutating it.
    if(m_outputSpec.meshes.use_count() > 1)
    {
      auto fresh = std::make_shared<ossia::mesh_list>();
      fresh->meshes = m_outputSpec.meshes->meshes;
      fresh->dirty_index = m_outputSpec.meshes->dirty_index;
      m_outputSpec.meshes = std::move(fresh);
    }

    auto wrapGpu = [](QRhiBuffer* b, int64_t size) {
      ossia::geometry::gpu_buffer gb;
      gb.handle = b;
      gb.byte_size = size;
      return ossia::geometry::buffer{.data = gb, .dirty = true};
    };

    bool any_emitted = false;
    for(auto& [key, b] : buckets)
    {
      if(b.draws.empty() || b.total_primitives == 0 || b.row_stride == 0)
        continue;

      auto& bb = m_primitiveCloudBuckets[key];
      bb.row_stride = b.row_stride;
      bb.last_seen_frame = m_primitiveCloudFrame;

      // ── Indirect-draw command shape (used both for size accounting
      // upfront and for the CPU build inside the upload guard).
      struct IndirectCmd
      {
        uint32_t indexOrVertexCount;
        uint32_t instanceCount;
        uint32_t firstIndexOrVertex;
        int32_t  baseVertex; // for indexed draws — unused (vertex_count path)
        uint32_t baseInstance;
      };

      // ── Upfront sizing (needed by growBuf AND by the per-bucket
      // geometry construction further down, which references the
      // owned buffer pointers regardless of upload/skip). raw_splats
      // needs VertexBuffer alongside StorageBuffer because the bucket
      // exposes the buffer through both paths: as an AUXILIARY SSBO
      // (CSF reads the row layout via std430) AND as a per-vertex
      // ATTRIBUTE buffer (Raw Raster's setVertexInput pulls every
      // g.input entry — even on procedural draws — and Vulkan
      // requires VK_BUFFER_USAGE_VERTEX_BUFFER_BIT for vertex
      // bindings).
      const int64_t rawBytes
          = (int64_t)b.total_primitives * (int64_t)b.row_stride;
      const uint32_t bucketCloudCount = (uint32_t)b.draws.size();
      const int64_t cmBytes
          = (int64_t)bucketCloudCount * (int64_t)sizeof(CloudMetaGPU);
      const int64_t lookupBytes
          = (int64_t)b.total_primitives * (int64_t)sizeof(uint32_t);
      const int64_t icBytes = (int64_t)sizeof(IndirectCmd);

      growBuf(renderer, res,bb.raw_splats, bb.rawSplatsCap, rawBytes,
              UF(QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer),
              "ScenePreprocessor::cloud.raw_splats");
      growBuf(renderer, res,bb.cloud_meta, bb.cloudMetaCap, cmBytes,
              UF(QRhiBuffer::StorageBuffer),
              "ScenePreprocessor::cloud.cloud_meta");
      growBuf(renderer, res,bb.cloud_id_lookup, bb.cloudIdLookupCap, lookupBytes,
              UF(QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer),
              "ScenePreprocessor::cloud.cloud_id_lookup");
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      growBuf(renderer, res,bb.indirect, bb.indirectCap, icBytes,
              UF(QRhiBuffer::StorageBuffer | QRhiBuffer::IndirectBuffer),
              "ScenePreprocessor::cloud.indirect");
#else
      growBuf(renderer, res,bb.indirect, bb.indirectCap, icBytes,
              UF(QRhiBuffer::StorageBuffer),
              "ScenePreprocessor::cloud.indirect");
#endif

      // ── Delta-update fingerprint ─────────────────────────────────
      // Hash everything the four GPU buffers depend on. When this
      // matches the last frame's value, the buckets are byte-equal
      // to what the previous frame uploaded — the per-frame CPU
      // concat + uploadStaticBuffer ×4 is pure waste, skip it.
      // For the user's "drop a 1 GB PLY into a static scene" case
      // this brings raw_splats per-frame work from ~720 MB/s of GPU
      // memcpy down to zero. The growBuf calls above already
      // short-circuited (cap >= need), so on the steady state the
      // entire bucket loop becomes O(draws.size()) hashing.
      uint64_t fp = 0;
      ossia::hash_combine(fp, (uint64_t)bucketCloudCount);
      ossia::hash_combine(fp, (uint64_t)b.row_stride);
      ossia::hash_combine(fp, (uint64_t)b.total_primitives);
      for(const auto* d : b.draws)
      {
        const auto* raw = d->cloud->raw_data.get();
        ossia::hash_combine(fp, (uint64_t)(uintptr_t)raw);
        // raw_data carries an explicit content_hash for fast
        // diff-skip when the producer can stamp one (PlyParser
        // sets it from the storage pointer); fall back to
        // dirty_index for producers that don't.
        const uint64_t content_id
            = raw ? (raw->content_hash != 0
                         ? raw->content_hash
                         : (uint64_t)raw->dirty_index)
                  : 0u;
        ossia::hash_combine(fp, content_id);
        ossia::hash_combine(fp, (uint64_t)d->cloud->primitive_count);
        ossia::hash_combine(fp, (uint64_t)d->transform_slot);
        // worldTransform: 16 floats × 4 = 64 bytes column-major.
        ossia::hash_combine(
            fp,
            ossia::hash_bytes(d->worldTransform.constData(), 64));
      }

      // 0 = "never uploaded yet, force the first frame's upload
      // regardless of fingerprint matching". growBuf may also have
      // just allocated a fresh VkBuffer (cap < need), in which case
      // the old data is gone; the fingerprint differs from frame N-1
      // because the size constraint changed (total_primitives or
      // row_stride is part of fp). Either way the !unchanged branch
      // runs and we re-upload.
      const bool unchanged = (bb.content_fingerprint != 0)
                             && (bb.content_fingerprint == fp)
                             && (bb.raw_splats != nullptr);

      if(!unchanged)
      {
        // ── raw_splats: concatenation of all clouds' raw bytes ────────
        // Bucket-internal format_id mismatch was rejected above so all
        // clouds in this bucket share row_stride.
        std::vector<uint8_t> concat;
        concat.resize((std::size_t)rawBytes);
        uint8_t* dst = concat.data();
        for(const auto* d : b.draws)
        {
          const auto& br = d->cloud->raw_data;
          if(!br) continue;
          const int64_t bytes
              = (int64_t)d->cloud->primitive_count * (int64_t)b.row_stride;
          if(auto* cpu = ossia::get_if<ossia::buffer_data>(&br->resource))
          {
            if(cpu->data && cpu->byte_size >= bytes)
            {
              std::memcpy(dst, cpu->data.get(), (std::size_t)bytes);
            }
            else
            {
              std::memset(dst, 0, (std::size_t)bytes);
            }
          }
          else
          {
            // GPU-resident raw_data: unsupported for now (would need a
            // GPU-to-GPU copy via copyBuffer). Zero-fill so the bucket
            // is at least well-defined. See PRIMITIVE-CLOUD-ARENA-DESIGN.md
            // for the planned slot-based path where GPU-resident
            // producers write into the per-format arena directly.
            std::memset(dst, 0, (std::size_t)bytes);
          }
          dst += bytes;
        }
        res.uploadStaticBuffer(bb.raw_splats, 0, rawBytes, concat.data());

        // ── cloud_meta + cloud_id_lookup ─────────────────────────────
        std::vector<CloudMetaGPU> cmData;
        cmData.resize(bucketCloudCount);

        std::vector<uint32_t> lookup;
        lookup.resize((std::size_t)b.total_primitives);

        uint32_t prim_offset = 0;
        uint32_t prim_lookup_pos = 0;
        for(uint32_t ci = 0; ci < bucketCloudCount; ++ci)
        {
          const auto* d = b.draws[ci];
          CloudMetaGPU& gm = cmData[ci];

          // Composed world matrix from the FlattenVisitor walk
          // (parentWorld). QMatrix4x4 is column-major and we want a
          // column-major float[16] — its constData() returns column-
          // major memory directly.
          const float* m = d->worldTransform.constData();
          for(int k = 0; k < 16; ++k) gm.model[k] = m[k];

          // Per-cloud world-space AABB: 8-corner walk of the local
          // bounds through worldTransform. Mirrors the bucket-bounds
          // loop below at :~1776, but kept per-cloud so format CSFs
          // can frustum-cull individual clouds inside a bucket.
          const auto& lb = d->cloud->bounds;
          if(lb.empty())
          {
            // Sentinel: empty bounds -> produce an inverted AABB so
            // any frustum test in the shader trivially marks it
            // visible (consumers can also check for the inversion).
            gm.bounds_min[0] = gm.bounds_min[1] = gm.bounds_min[2] = 1.f;
            gm.bounds_max[0] = gm.bounds_max[1] = gm.bounds_max[2] = -1.f;
          }
          else
          {
            const QMatrix4x4& W = d->worldTransform;
            float minx = std::numeric_limits<float>::infinity();
            float miny = minx, minz = minx;
            float maxx = -minx, maxy = -minx, maxz = -minx;
            for(int corner = 0; corner < 8; ++corner)
            {
              const float x = (corner & 1) ? lb.max[0] : lb.min[0];
              const float y = (corner & 2) ? lb.max[1] : lb.min[1];
              const float z = (corner & 4) ? lb.max[2] : lb.min[2];
              const QVector3D p = W.map(QVector3D(x, y, z));
              minx = std::min(minx, p.x()); maxx = std::max(maxx, p.x());
              miny = std::min(miny, p.y()); maxy = std::max(maxy, p.y());
              minz = std::min(minz, p.z()); maxz = std::max(maxz, p.z());
            }
            gm.bounds_min[0] = minx; gm.bounds_min[1] = miny; gm.bounds_min[2] = minz;
            gm.bounds_max[0] = maxx; gm.bounds_max[1] = maxy; gm.bounds_max[2] = maxz;
          }
          gm.bounds_min[3] = 0.f;
          gm.bounds_max[3] = 0.f;

          gm.primitive_offset    = prim_offset;
          gm.primitive_count     = (uint32_t)d->cloud->primitive_count;
          gm.transform_slot      = d->transform_slot; // 0xFFFFFFFFu = none
          gm.format_param_index  = 0; // unused for v1
          gm._pad[0] = gm._pad[1] = gm._pad[2] = gm._pad[3] = 0;

          // Fill lookup[prim_offset..prim_offset+count] = ci
          for(uint32_t p = 0; p < gm.primitive_count; ++p)
            lookup[prim_lookup_pos + p] = ci;
          prim_lookup_pos += gm.primitive_count;
          prim_offset    += gm.primitive_count;
        }

        res.uploadStaticBuffer(
            bb.cloud_meta, 0, cmBytes, cmData.data());
        res.uploadStaticBuffer(
            bb.cloud_id_lookup, 0, lookupBytes, lookup.data());

        // ── indirect_draw_cmds: one cmd, vertex_count=N (one slot per
        // primitive). The bucket geometry is a flat point cloud — the
        // CSF stage downstream (e.g. 01_Decode for 3dgs.classic) reads
        // `$VERTEX_COUNT_geoIn = N` and emits the instanced 6×N quad
        // topology its draw stage expects. Format CSF chains may rewrite
        // this cmd post-cull to shrink the active set; the unculled
        // total is the safe default.
        const IndirectCmd cmd{
            /*indexOrVertexCount*/ (uint32_t)b.total_primitives,
            /*instanceCount*/      1u,
            /*firstIndexOrVertex*/ 0u,
            /*baseVertex*/         0,
            /*baseInstance*/       0u};
        res.uploadStaticBuffer(bb.indirect, 0, icBytes, &cmd);

        bb.content_fingerprint = fp;
      }

      // ── Build the bucket geometry ─────────────────────────────────
      ossia::geometry g;
      const int rawSplatsBufIdx     = (int)g.buffers.size();
      g.buffers.push_back(wrapGpu(bb.raw_splats, rawBytes));
      const int cloudMetaBufIdx     = (int)g.buffers.size();
      g.buffers.push_back(wrapGpu(bb.cloud_meta, cmBytes));
      const int cloudLookupBufIdx   = (int)g.buffers.size();
      g.buffers.push_back(wrapGpu(bb.cloud_id_lookup, lookupBytes));
      const int indirectBufIdx      = (int)g.buffers.size();
      g.buffers.push_back(wrapGpu(bb.indirect, icBytes));

      g.auxiliary.push_back({
          .name = "raw_splats",
          .buffer = rawSplatsBufIdx,
          .byte_offset = 0, .byte_size = rawBytes});
      g.auxiliary.push_back({
          .name = "cloud_meta",
          .buffer = cloudMetaBufIdx,
          .byte_offset = 0, .byte_size = cmBytes});

      // Expose the cloud→primitive mapping as a per-vertex ATTRIBUTE
      // (one uint per primitive), not as AUXILIARY. The CSF binder
      // converts ATTRIBUTES into named SSBOs accessible as
      // `geo_cloud_id_in[idx]`, and — crucially — the presence of a
      // read_only ATTRIBUTE on the input geometry resource is what
      // makes the CSF node *create an input port*. Without at least
      // one such attribute the node has no way to be wired up.
      ossia::geometry::binding cidBinding{};
      cidBinding.byte_stride    = 4;
      cidBinding.classification = ossia::geometry::binding::per_vertex;
      const int cidBindingIdx = (int)g.bindings.size();
      g.bindings.push_back(cidBinding);

      struct ossia::geometry::input cidInput{};
      cidInput.buffer      = cloudLookupBufIdx;
      cidInput.byte_offset = 0;
      g.input.push_back(cidInput);

      ossia::geometry::attribute cidAttr{};
      cidAttr.binding     = cidBindingIdx;
      cidAttr.location    = 0;
      cidAttr.format      = ossia::geometry::attribute::uint1;
      cidAttr.byte_offset = 0;
      cidAttr.semantic    = ossia::attribute_semantic::custom;
      cidAttr.name        = "cloud_id";
      g.attributes.push_back(cidAttr);

      // When the producer named a struct type for the per-row payload
      // (e.g. PlyParser sets "Splat3DGS" for 3dgs.classic), expose
      // raw_splats *also* as a per-vertex ATTRIBUTE of format
      // user_struct. The CSF binder generates a `Splat3DGS
      // geo_splat_in[]` SSBO declaration matching the consumer's
      // `TYPES.Splat3DGS` block, so shaders read rows as
      // `ISF_READ(geoIn, splat)[idx].field` directly. The legacy
      // raw_splats AUXILIARY entry above stays so older presets keep
      // working through the migration; once all bundled presets move
      // to TYPES the AUXILIARY emit can drop.
      const auto* rep = b.draws[0]->cloud.get();
      if(rep && !rep->struct_type_name.empty())
      {
        ossia::geometry::binding splatBinding{};
        splatBinding.byte_stride    = (uint32_t)b.row_stride;
        splatBinding.classification = ossia::geometry::binding::per_vertex;
        const int splatBindingIdx = (int)g.bindings.size();
        g.bindings.push_back(splatBinding);

        struct ossia::geometry::input splatInput{};
        splatInput.buffer      = rawSplatsBufIdx;
        splatInput.byte_offset = 0;
        g.input.push_back(splatInput);

        ossia::geometry::attribute splatAttr{};
        splatAttr.binding           = splatBindingIdx;
        splatAttr.location          = 1;
        splatAttr.format            = ossia::geometry::attribute::user_struct;
        splatAttr.element_byte_size = (uint32_t)b.row_stride;
        splatAttr.user_type_name    = rep->struct_type_name;
        splatAttr.byte_offset       = 0;
        splatAttr.semantic          = ossia::attribute_semantic::custom;
        splatAttr.name              = "splat";
        g.attributes.push_back(splatAttr);
      }

      // Forward the camera UBO (uploaded earlier in update() before
      // rebuildMDI) so cloud-format CSF stages can read view /
      // projection / cameraPosition / renderSize without manual
      // wiring. Same name ("camera") that mesh shaders use, so a
      // single GLSL UBO declaration works for both paths.
      if(m_camerasBuffer)
      {
        const int camBufIdx = (int)g.buffers.size();
        g.buffers.push_back(
            wrapGpu(m_camerasBuffer, (int64_t)sizeof(CameraUBOData)));
        g.auxiliary.push_back({
            .name = "camera",
            .buffer = camBufIdx,
            .byte_offset = 0,
            .byte_size = (int64_t)sizeof(CameraUBOData)});
      }
      if(m_sceneCountsBuffer)
      {
        const int countsBufIdx = (int)g.buffers.size();
        g.buffers.push_back(
            wrapGpu(m_sceneCountsBuffer, (int64_t)sizeof(SceneCountsUBO)));
        g.auxiliary.push_back({
            .name = "scene_counts",
            .buffer = countsBufIdx,
            .byte_offset = 0,
            .byte_size = (int64_t)sizeof(SceneCountsUBO)});
      }

      // Indirect draw shape: vertex_count=N points, instance_count=1.
      // The bucket is a flat point cloud — instancing is introduced by
      // the format's CSF preprocessor (which converts each input
      // "vertex" into a 6-vertex×N-instance quad topology its raster
      // stage consumes).
      ossia::geometry::gpu_buffer ic_gpu;
      ic_gpu.handle = bb.indirect;
      ic_gpu.byte_size = icBytes;
      g.indirect_count = ic_gpu;

      // Mirror the IndirectCmd shape uploaded inside the !unchanged guard
      // (or kept stable from a previous frame). Values are derived directly
      // from b.total_primitives + the bucket's "one cmd, instance=1" shape;
      // re-deriving here avoids hoisting `cmd` itself out of the upload
      // guard just to read its fields.
      g.cpu_draw_commands.push_back({
          .index_or_vertex_count = (uint32_t)b.total_primitives,
          .instance_count        = 1u,
          .first_index_or_vertex = 0u,
          .base_vertex           = 0,
          .first_instance        = 0u});

      g.vertices  = (int)b.total_primitives;
      g.instances = 1;
      g.topology  = ossia::geometry::points;
      g.cull_mode = ossia::geometry::none;
      g.front_face = ossia::geometry::counter_clockwise;
      // Splats need alpha-blend; tag the geometry so a downstream
      // RawRaster picks the right pipeline state. The format's actual
      // PIPELINE_STATE in its .frag overrides this if more specific.
      g.blend = ossia::geometry::blend_premultiplied_alpha;
      g.depth_write = false;

      // Surface format_id as filter_tag (rapidhash truncated to 32 bits)
      // so a downstream FlattenedSceneFilterNode in "format_id ==
      // match_str" mode can route this bucket to its format-specific
      // shader chain. Same hash that the bucket key above uses, so the
      // producer-side bucketing and the consumer-side filter agree
      // byte-for-byte. Empty format_id leaves filter_tag at 0 (the
      // "untagged" sentinel — string-match mode treats both as "no
      // tag" and matches when match_str is also empty).
      if(rep && !rep->format_id.empty())
        g.filter_tag = (uint32_t)ossia::hash_string(rep->format_id);

      // Bounds: union of cloud world-space AABBs.
      ossia::aabb worldBounds{};
      worldBounds.min[0] = worldBounds.min[1] = worldBounds.min[2] = 1.f;
      worldBounds.max[0] = worldBounds.max[1] = worldBounds.max[2] = -1.f;
      for(const auto* d : b.draws)
      {
        const auto& lb = d->cloud->bounds;
        if(lb.empty())
          continue;
        // 8 corners of the local AABB transformed to world space.
        const QMatrix4x4& W = d->worldTransform;
        for(int corner = 0; corner < 8; ++corner)
        {
          const float x = (corner & 1) ? lb.max[0] : lb.min[0];
          const float y = (corner & 2) ? lb.max[1] : lb.min[1];
          const float z = (corner & 4) ? lb.max[2] : lb.min[2];
          // Use QMatrix4x4::map() (inline member, no QtGui operator
          // export needed). Equivalent to (W * vec4(x,y,z,1)).xyz.
          const QVector3D p = W.map(QVector3D(x, y, z));
          worldBounds.expand(p.x(), p.y(), p.z());
        }
      }
      if(!worldBounds.empty())
      {
        g.bounds.min[0] = worldBounds.min[0];
        g.bounds.min[1] = worldBounds.min[1];
        g.bounds.min[2] = worldBounds.min[2];
        g.bounds.max[0] = worldBounds.max[0];
        g.bounds.max[1] = worldBounds.max[1];
        g.bounds.max[2] = worldBounds.max[2];
      }

      m_outputSpec.meshes->meshes.push_back(std::move(g));
      any_emitted = true;
    }

    if(any_emitted)
    {
      m_outputSpec.meshes->dirty_index += 1;
    }
  }

  void rebuildMDI(
      RenderList& renderer, QRhiResourceUpdateBatch& res, const FlatScene& fs,
      const std::vector<uint32_t>& materialTagHashes)
  {
    // Per-mesh slab allocation replaces the old concat-and-bulk-upload
    // path. Flow per draw:
    //   1. acquireMeshSlab(stable_id, vc, ic) — hit OR fresh allocation
    //      into the 5 per-stream OffsetAllocators in GpuResourceRegistry.
    //   2. If slab.freshly_allocated: extract CPU bytes (or queue a GPU
    //      copy for GPU-backed sources) and uploadMeshStream into the
    //      slab's byte offset on each stream. Existing slabs: zero upload.
    //   3. indirect_draw_cmds baseVertex / firstIndex come from the slab's
    //      byte offsets divided by stream stride.
    //   4. markMeshSlabSeen so the per-frame sweep doesn't reclaim it.
    // The grace queue (2 frames by default) prevents the arena from
    // returning a live slab's offset to another allocation while an
    // in-flight draw still references it.
    //
    // Output layout is unchanged: four vertex bindings (pos/nrm/uv/tan)
    // + one index buffer + all the scene auxiliaries. Consumer shaders
    // see identical output shape.
    //
    // What's NOT in this function anymore:
    //  - Concatenated CPU byte vectors (acc.positions / .normals / …).
    //  - Running baseVertex / firstIndex counters.
    //  - uploadStaticBuffer(offset=0, totalBytes) for vertex/index streams
    //    — those buffers are registry-owned; we write per-slab only.
    //  - growBuf for vertex/index streams — pre-sized at registry init.
    // What IS here: the per_draws + indirect_draw_cmds upload (small
    // preprocessor-owned SSBOs), per-draw metadata pack, output
    // geometry construction.
    auto& rhi = *renderer.state.rhi;
    const uint32_t current_frame = (uint32_t)renderer.frame;

    struct Acc
    {
      std::vector<PerDrawGPU> perDraws;
      std::vector<PerDrawBoundsGPU> perDrawBounds;
      struct IndirectCmd
      {
        uint32_t indexCount, instanceCount, firstIndex;
        int32_t baseVertex;
        uint32_t baseInstance;
      };
      std::vector<IndirectCmd> indirectCmds;
    } acc;

    acc.perDraws.reserve(std::max(m_lastDrawCount, fs.draws.size()));
    acc.perDrawBounds.reserve(std::max(m_lastDrawCount, fs.draws.size()));
    acc.indirectCmds.reserve(std::max(m_lastDrawCount, fs.draws.size()));

    // Concat-offsets for joint matrices across all skeletons in this
    // flatten. skinJointOffsets[k] = sum of joint counts for skins < k.
    // Stamped into PerDrawGPU.skeleton_offset so a future consolidated
    // `joint_matrices` SSBO (single buffer across all skeletons) is a
    // drop-in change on the shader side — offsets already point at the
    // correct record. 0xFFFFFFFF sentinel is written for unskinned
    // draws.
    std::vector<uint32_t> skinJointOffsets;
    skinJointOffsets.reserve(fs.skins.size());
    {
      uint32_t running = 0;
      for(const auto& sk : fs.skins)
      {
        skinJointOffsets.push_back(running);
        running += (uint32_t)sk.joint_matrices.size();
      }
    }

    // Reset pending GPU copies for this frame — populated below when a
    // draw's attributes are GPU-resident; issued in runInitialPasses.
    m_pendingGpuCopies.clear();

    // Queue one copy op targeting a slab's byte offset in the arena
    // stream. No accumulator pre-reservation here: dst_offset is the
    // slab's allocator-assigned offset, not an accumulator-relative
    // position.
    auto queueSlabCopy = [&](MdiAttr attr, const GpuAttrView& view,
                             int elem_size, int vertex_count,
                             uint32_t dst_slab_offset) {
      PendingGpuCopy op;
      op.attr = attr;
      op.src = view.buf;
      op.src_offset = view.src_offset;
      op.dst_offset = (int)dst_slab_offset;
      op.vertex_count = vertex_count;
      op.src_stride = view.byte_stride;
      op.element_size = elem_size;
      op.size = (op.src_stride == 0 || op.src_stride == elem_size)
                    ? vertex_count * elem_size
                    : elem_size; // per-vertex path computes size each iter
      m_pendingGpuCopies.push_back(op);
    };

    // Scratch CPU buffers reused across draws to hold the padded
    // vec3→vec4 conversions for positions / normals and the fallback
    // (1,0,0,1) tangents. Grow-only; never shrinks. Avoids re-allocating
    // for each per-draw upload.
    std::vector<std::byte> scratch;

    uint32_t totalVertices = 0;
    uint32_t totalIndices = 0;
    bool warned_missing_stable_id = false;

    using Stream = GpuResourceRegistry::MeshStream;

    // Running cursor into the unified per-instance concat space. Each
    // emitted indirect cmd consumes `instanceCount` contiguous slots and
    // writes its own cmd-index into draw_ids[slot..slot+instanceCount-1].
    // For regular fs.draws cmds (instanceCount=1) cmd_index == slot
    // index. For fs.instances groups (instanceCount=N) cmd_index !=
    // slot index, so the shader CANNOT use gl_BaseInstance/gl_DrawID to
    // recover the cmd index — it reads the per-instance `draw_id`
    // attribute that this cursor populates.
    uint32_t slot_cursor = 0;

    // Records of instance-group slot ranges so the post-loop CPU
    // bookkeeping can pre-fill draw_ids and queue the GPU copies for
    // upstream translation / color buffers into the right concat
    // offsets without a second pass over fs.instances.
    struct InstanceSlotRecord
    {
      uint32_t slot_base;
      uint32_t count;
      uint32_t cmd_index;
      QRhiBuffer* src_translations;
      uint32_t src_translation_offset;
      uint32_t src_translation_stride;
      QRhiBuffer* src_colors;
      uint32_t src_color_offset;
    };
    std::vector<InstanceSlotRecord> instanceRecords;

    // Shared per-cmd processor. Used by the fs.draws loop and the
    // fs.instances loop. Performs:
    //   - attribute extraction (CPU + GPU paths) from the wrapper
    //     ossia::geometry
    //   - slab acquire / per-stream upload (only on freshly_allocated)
    //   - per_draws + per_draw_bounds push
    //   - indirect cmd push with firstInstance = slot_cursor
    //   - slot_cursor += instanceCount
    // Returns the cmd_index that was emitted (== acc.indirectCmds.size()
    // BEFORE the push, == sentinel if the cmd was skipped).
    constexpr uint32_t kCmdSkipped = 0xFFFFFFFFu;
    auto emitDraw = [&](
        const ossia::geometry* mesh, uint64_t stable_id,
        const QMatrix4x4& worldTransform,
        const ossia::material_component* materialPtr,
        int materialIndex, uint32_t transform_slot,
        int skinIndex, const ossia::aabb& local_bounds,
        uint32_t instanceCount) -> uint32_t
    {
      if(!mesh || mesh->vertices <= 0 || !m_registry || instanceCount == 0)
        return kCmdSkipped;
      if(stable_id == 0)
      {
        if(!warned_missing_stable_id)
        {
          qWarning() << "ScenePreprocessor::rebuildMDI: draw has no "
                        "stable_id — synthesising from mesh pointer. "
                        "Producer should stamp mesh_primitive::stable_id "
                        "for cache stability.";
          warned_missing_stable_id = true;
        }
        stable_id = (uint64_t)((uintptr_t)mesh)
                    ^ ((uint64_t)mesh->vertices << 32)
                    ^ (uint64_t)mesh->indices;
        if(stable_id == 0)
          stable_id = 1;
      }

      // CPU extraction — still the hot path for loaded glTF/FBX scenes.
      auto pos = extractCpuAttribute<12>(*mesh, ossia::attribute_semantic::position);
      auto nrm = extractCpuAttribute<12>(*mesh, ossia::attribute_semantic::normal);
      auto uv  = extractCpuAttribute<8>(*mesh, ossia::attribute_semantic::texcoord0);
      auto uv1 = extractCpuAttribute<8>(*mesh, ossia::attribute_semantic::texcoord1);
      auto col = extractCpuAttribute<16>(*mesh, ossia::attribute_semantic::color0);
      auto tan = extractCpuAttribute<16>(*mesh, ossia::attribute_semantic::tangent);

      GpuAttrView gpu_pos, gpu_nrm, gpu_uv, gpu_tan;
      if(pos.empty())
        gpu_pos = extractGpuAttribute(*mesh, ossia::attribute_semantic::position);
      if(nrm.empty())
        gpu_nrm = extractGpuAttribute(*mesh, ossia::attribute_semantic::normal);
      if(uv.empty())
        gpu_uv = extractGpuAttribute(*mesh, ossia::attribute_semantic::texcoord0);
      if(tan.empty())
        gpu_tan = extractGpuAttribute(*mesh, ossia::attribute_semantic::tangent);

      if(pos.empty() && !gpu_pos.buf)
        return kCmdSkipped;

      std::vector<uint32_t> idx;
      if(mesh->indices > 0)
      {
        idx = extractCpuIndices(*mesh);
        if(idx.empty())
          return kCmdSkipped; // GPU-backed indices not yet supported.
      }
      else
      {
        idx.resize(mesh->vertices);
        for(int v = 0; v < mesh->vertices; ++v)
          idx[v] = (uint32_t)v;
      }

      const uint32_t drawIndexCount = (uint32_t)idx.size();
      const int vc = mesh->vertices;

      auto* slab = m_registry->acquireMeshSlab(
          stable_id, (uint32_t)vc, drawIndexCount, current_frame);
      if(!slab)
        return kCmdSkipped;

      m_registry->markMeshSlabSeen(stable_id, current_frame);

      if(slab->freshly_allocated)
      {
        // ── Position ── vec3→vec4 padding when CPU-sourced.
        const uint32_t posOff
            = m_registry->meshSlabOffsetBytes(*slab, Stream::Positions);
        if(!pos.empty())
        {
          scratch.assign(std::size_t(vc) * 16, std::byte{});
          for(int v = 0; v < vc; ++v)
            std::memcpy(scratch.data() + v * 16, pos.data() + v * 12, 12);
          m_registry->uploadMeshStream(
              res, *slab, Stream::Positions,
              scratch.data(), (uint32_t)scratch.size());
        }
        else
        {
          queueSlabCopy(MdiAttr::Positions, gpu_pos, 16, vc, posOff);
        }

        // ── Normals ── vec3→vec4 padding; zero fallback when missing.
        const uint32_t nrmOff
            = m_registry->meshSlabOffsetBytes(*slab, Stream::Normals);
        if(!nrm.empty())
        {
          scratch.assign(std::size_t(vc) * 16, std::byte{});
          for(int v = 0; v < vc; ++v)
            std::memcpy(scratch.data() + v * 16, nrm.data() + v * 12, 12);
          m_registry->uploadMeshStream(
              res, *slab, Stream::Normals,
              scratch.data(), (uint32_t)scratch.size());
        }
        else if(gpu_nrm.buf)
        {
          queueSlabCopy(MdiAttr::Normals, gpu_nrm, 16, vc, nrmOff);
        }
        else
        {
          scratch.assign(std::size_t(vc) * 16, std::byte{});
          m_registry->uploadMeshStream(
              res, *slab, Stream::Normals,
              scratch.data(), (uint32_t)scratch.size());
        }

        // ── Texcoords ── vec2; zero fallback when missing.
        const uint32_t uvOff
            = m_registry->meshSlabOffsetBytes(*slab, Stream::Texcoords);
        if(!uv.empty())
        {
          m_registry->uploadMeshStream(
              res, *slab, Stream::Texcoords,
              uv.data(), (uint32_t)uv.size());
        }
        else if(gpu_uv.buf)
        {
          queueSlabCopy(MdiAttr::Texcoords, gpu_uv, 8, vc, uvOff);
        }
        else
        {
          scratch.assign(std::size_t(vc) * 8, std::byte{});
          m_registry->uploadMeshStream(
              res, *slab, Stream::Texcoords,
              scratch.data(), (uint32_t)scratch.size());
        }

        // ── Tangents ── vec4; (1,0,0,1) fallback.
        const uint32_t tanOff
            = m_registry->meshSlabOffsetBytes(*slab, Stream::Tangents);
        if(!tan.empty())
        {
          m_registry->uploadMeshStream(
              res, *slab, Stream::Tangents,
              tan.data(), (uint32_t)tan.size());
        }
        else if(gpu_tan.buf)
        {
          queueSlabCopy(MdiAttr::Tangents, gpu_tan, 16, vc, tanOff);
        }
        else
        {
          scratch.assign(std::size_t(vc) * 16, std::byte{});
          float fb[4] = {1.f, 0.f, 0.f, 1.f};
          for(int v = 0; v < vc; ++v)
            std::memcpy(scratch.data() + v * 16, fb, 16);
          m_registry->uploadMeshStream(
              res, *slab, Stream::Tangents,
              scratch.data(), (uint32_t)scratch.size());
        }

        // ── Colors ── vec4; (1,1,1,1) fallback.
        if(!col.empty())
        {
          m_registry->uploadMeshStream(
              res, *slab, Stream::Colors,
              col.data(), (uint32_t)col.size());
        }
        else
        {
          scratch.assign(std::size_t(vc) * 16, std::byte{});
          float fb[4] = {1.f, 1.f, 1.f, 1.f};
          for(int v = 0; v < vc; ++v)
            std::memcpy(scratch.data() + v * 16, fb, 16);
          m_registry->uploadMeshStream(
              res, *slab, Stream::Colors,
              scratch.data(), (uint32_t)scratch.size());
        }

        // ── Texcoords1 ── vec2; zero fallback.
        if(!uv1.empty())
        {
          m_registry->uploadMeshStream(
              res, *slab, Stream::Texcoords1,
              uv1.data(), (uint32_t)uv1.size());
        }
        else
        {
          scratch.assign(std::size_t(vc) * 8, std::byte{});
          m_registry->uploadMeshStream(
              res, *slab, Stream::Texcoords1,
              scratch.data(), (uint32_t)scratch.size());
        }

        // ── Indices ──
        m_registry->uploadMeshStream(
            res, *slab, Stream::Indices,
            idx.data(), (uint32_t)(idx.size() * 4));
      }

      // Per-draw GPU record.
      PerDrawGPU pd{};
      writeMat4(pd.model, worldTransform);
      QMatrix4x4 nm = worldTransform.inverted().transposed();
      nm.setColumn(3, QVector4D(0, 0, 0, 1));
      nm.setRow(3, QVector4D(0, 0, 0, 1));
      writeMat4(pd.normal, nm);
      pd.material_index = arenaSlotForMaterial(materialPtr);
      pd.tag_hash
          = (materialIndex >= 0
             && (std::size_t)materialIndex < materialTagHashes.size())
              ? materialTagHashes[(std::size_t)materialIndex]
              : 0u;
      pd.transform_slot = transform_slot;
      pd.skeleton_offset
          = (skinIndex >= 0
             && (std::size_t)skinIndex < skinJointOffsets.size())
                ? skinJointOffsets[(std::size_t)skinIndex]
                : 0xFFFFFFFFu;
      acc.perDraws.push_back(pd);
      acc.perDrawBounds.push_back(packBounds(local_bounds));

      const uint32_t cmd_index = (uint32_t)acc.indirectCmds.size();
      Acc::IndirectCmd cmd{
          drawIndexCount,
          instanceCount,
          slab->index_slot.offset,
          (int32_t)slab->vertex_slot.offset,
          slot_cursor};
      acc.indirectCmds.push_back(cmd);
      slot_cursor += instanceCount;

      totalVertices += (uint32_t)vc;
      totalIndices += drawIndexCount;
      return cmd_index;
    };

    for(std::size_t i = 0; i < fs.draws.size(); ++i)
    {
      const auto& dc = fs.draws[i];
      emitDraw(
          dc.mesh, dc.stable_id, dc.worldTransform, dc.material.get(),
          dc.materialIndex, dc.transform_slot, dc.skinIndex, dc.local_bounds,
          /*instanceCount=*/1u);
    }

    // Number of per_draws entries that the fs.draws loop actually emitted
    // (i.e. after emitDraw's skip predicate). The fast path's diff-upload
    // mirror must be seeded from exactly this prefix — emitDraw can skip
    // draws (slab exhaustion, GPU-backed indices, missing positions) that a
    // naive `vertices > 0` filter would wrongly keep, which would desync the
    // mirror from the GPU per_draws layout.
    const std::size_t meshDrawCount = acc.perDraws.size();

    // ── fs.instances ── one cmd per instance_component, instanceCount =
    // group's instance count, firstInstance = slot_cursor before the
    // cmd. Per-instance translations / colors are GPU-copied from the
    // upstream Instancer's source buffers into the concat per-instance
    // arrays at offset slot_base * stride; CPU-side draw_ids[slot..]
    // get the cmd-index of the owning group (populated below, after
    // both loops complete and slot_cursor stops moving).
    //
    // Defensive null-handle skip: the upstream Instancer may republish
    // a fresh `instance_component` whose buffer handles haven't been
    // populated yet (CSF compute pass mid-rebuild, etc). Skipping the
    // group for that frame is correct — next frame the upstream is
    // ready and the group renders.
    for(std::size_t k = 0; k < fs.instances.size(); ++k)
    {
      const auto& inst_draw = fs.instances[k];
      if(!inst_draw.instance)
        continue;
      const auto& inst = *inst_draw.instance;
      if(!inst.prototype || inst.prototype->primitives.empty())
        continue;
      if(inst.instance_count == 0)
        continue;

      const auto& prim = inst.prototype->primitives[0];
      if(prim.vertex_count == 0)
        continue;

      // Defensive null-handle skip on prototype buffers — happens during
      // model swaps when the new prototype's data hasn't been uploaded
      // yet. The next frame retries.
      bool prototype_buffers_ready = true;
      for(const auto& vb : prim.vertex_buffers)
      {
        if(!vb)
          continue;
        if(auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(&vb->resource))
        {
          if(!gpu->native_handle)
          { prototype_buffers_ready = false; break; }
        }
        else if(auto* cpu = ossia::get_if<ossia::buffer_data>(&vb->resource))
        {
          if(!cpu->data || cpu->byte_size == 0)
          { prototype_buffers_ready = false; break; }
        }
        else
        { prototype_buffers_ready = false; break; }
      }
      if(prim.index_buffer && prototype_buffers_ready)
      {
        const auto& ib = *prim.index_buffer;
        if(auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(&ib.resource))
        {
          if(!gpu->native_handle) prototype_buffers_ready = false;
        }
        else if(auto* cpu = ossia::get_if<ossia::buffer_data>(&ib.resource))
        {
          if(!cpu->data || cpu->byte_size == 0) prototype_buffers_ready = false;
        }
      }
      if(!prototype_buffers_ready)
        continue;

      // Per-instance source buffers — translations may carry vec3 / trs /
      // mat4 layouts; we currently only support `translation` (the
      // shader's per-instance VERTEX_INPUT is vec3). trs / mat4 support
      // is a follow-up.
      QRhiBuffer* srcTranslations = nullptr;
      uint32_t srcTranslationOffset = 0;
      uint32_t srcTranslationStride = 16; // CSF emitters pad to vec4.
      // Per-format byte offset of the translation within the source
      // element. For column-major mat4 (64 B), the translation is
      // column 3 at offset 48; vec4 / trs put translation at offset 0.
      uint32_t srcTranslationColumnOffset = 0;
      if(inst.instance_transforms)
      {
        if(auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(
               &inst.instance_transforms->resource))
        {
          if(!gpu->native_handle)
            continue;
          srcTranslations = static_cast<QRhiBuffer*>(gpu->native_handle);
          srcTranslationOffset = (uint32_t)gpu->byte_offset;
          using TF = ossia::instance_component::transform_format;
          switch(inst.transform_type)
          {
            case TF::translation: srcTranslationStride = 16; break;
            case TF::trs:         srcTranslationStride = 40; break;
            case TF::mat4:
              srcTranslationStride = 64;
              srcTranslationColumnOffset = 48;
              break;
          }
        }
      }
      QRhiBuffer* srcColors = nullptr;
      uint32_t srcColorOffset = 0;
      if(inst.instance_colors)
      {
        if(auto* gpu = ossia::get_if<ossia::gpu_buffer_handle>(
               &inst.instance_colors->resource))
        {
          if(!gpu->native_handle)
            continue;
          srcColors = static_cast<QRhiBuffer*>(gpu->native_handle);
          srcColorOffset = (uint32_t)gpu->byte_offset;
        }
      }

      // Build a transient ossia::geometry from the prototype primitive
      // and feed it into the shared emitDraw closure.
      auto proto_geom = primitiveToGeometry(prim);
      if(!proto_geom)
        continue;

      const uint32_t slot_base = slot_cursor;
      const uint64_t prim_id = resolvePrototypeStableId(
          inst.prototype.get(), prim);

      const uint32_t cmd_index = emitDraw(
          proto_geom.get(), prim_id, inst_draw.worldTransform,
          prim.material.get(), /*materialIndex=*/-1,
          inst.raw_slot.size != 0 ? inst.raw_slot.internal_index
                                  : 0xFFFFFFFFu,
          /*skinIndex=*/-1, prim.bounds, inst.instance_count);
      if(cmd_index == kCmdSkipped)
        continue;

      InstanceSlotRecord rec{};
      rec.slot_base = slot_base;
      rec.count = inst.instance_count;
      rec.cmd_index = cmd_index;
      rec.src_translations = srcTranslations;
      rec.src_translation_offset = srcTranslationOffset + srcTranslationColumnOffset;
      rec.src_translation_stride = srcTranslationStride;
      rec.src_colors = srcColors;
      rec.src_color_offset = srcColorOffset;
      instanceRecords.push_back(rec);
    }

    // GC slabs not seen this frame. Grace = 2 protects against the CB
    // still referencing a culled slab's offset through its indirect-
    // draw-cmds entry from frame N-1.
    m_registry->sweepMeshSlabs(current_frame, 2u);

    // Garbage-collect prototype-id map entries that no longer appear in
    // the live scene. Keeps the map bounded across long sessions where
    // Instancer prototypes get swapped (Box.gltf → Duck.gltf etc).
    {
      ossia::hash_set<const ossia::mesh_component*> live_protos;
      live_protos.reserve(fs.instances.size());
      for(const auto& id : fs.instances)
      {
        if(id.instance && id.instance->prototype)
          live_protos.insert(id.instance->prototype.get());
      }
      for(auto it = m_protoStableIds.begin(); it != m_protoStableIds.end();)
      {
        if(live_protos.find(it->first) == live_protos.end())
          it = m_protoStableIds.erase(it);
        else
          ++it;
      }
    }

    m_mdi.totalVertices = totalVertices;
    m_mdi.totalIndices = totalIndices;
    m_mdi.drawCount = (uint32_t)acc.indirectCmds.size();
    m_lastDrawCount = std::max(m_lastDrawCount, acc.indirectCmds.size());
    m_instSlotsUsed = slot_cursor;

    // drawCount==0: no mesh draws this frame, but procedural-only consumers
    // (classic_skybox, fullscreen-triangle effects) still need the
    // scene-wide aux table — `camera` rides on the geometry, so an empty
    // mesh_list would leave them with no camera UBO. Fall through and
    // build a 0-vertex carrier mesh that exposes the full auxiliary
    // list; mesh-consuming downstream nodes see vertices==0 and skip
    // their draw call. The drawCount-dependent uploads below are gated
    // on non-empty sources; the binding extents fall back to one
    // element so RHI accepts the bindings.

    const int64_t pdBytes = std::max<int64_t>(
        sizeof(PerDrawGPU),
        (int64_t)acc.perDraws.size() * sizeof(PerDrawGPU));
    const int64_t icBytes = std::max<int64_t>(
        sizeof(Acc::IndirectCmd),
        (int64_t)acc.indirectCmds.size() * sizeof(Acc::IndirectCmd));
    const int64_t pdbBytes
        = (int64_t)acc.perDrawBounds.size() * sizeof(PerDrawBoundsGPU);

    // Grow-only for the preprocessor-owned small SSBOs (arena streams
    // don't grow — pre-sized in registry.init()). On realloc we drop
    // the diff-upload mirror so the next diffUpload call (fast path
    // at lines 4744 / 4751) treats the new buffer as empty and uploads
    // the full fresh contents — see growBuf's prefix-staleness comment.
    // The slow path's `uploadStaticBuffer(per_draws, 0, full_size, ...)`
    // at lines 2478-2486 already covers a slow-frame realloc; the
    // mirror clear here defends the (less common) case where a fast
    // frame's grow is followed by another fast-frame diffUpload before
    // a slow frame intervenes.
    using UF = QRhiBuffer::UsageFlags;
    if(growBuf(renderer, res,m_mdi.per_draws, m_mdi.perDrawsCap, pdBytes,
               QRhiBuffer::StorageBuffer,
               "ScenePreprocessor::mdi.per_draws"))
      m_cachedPerDraws.clear();
    if(growBuf(renderer, res,m_mdi.per_draw_bounds, m_mdi.perDrawBoundsCap, pdbBytes,
               QRhiBuffer::StorageBuffer,
               "ScenePreprocessor::mdi.per_draw_bounds"))
      m_cachedPerDrawBounds.clear();
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    growBuf(renderer, res,m_mdi.indirect_draw_cmds, m_mdi.indirectCap, icBytes,
            UF(QRhiBuffer::StorageBuffer | QRhiBuffer::IndirectBuffer),
            "ScenePreprocessor::mdi.indirect_draw_cmds");
#else
    growBuf(renderer, res,m_mdi.indirect_draw_cmds, m_mdi.indirectCap, icBytes,
            QRhiBuffer::StorageBuffer,
            "ScenePreprocessor::mdi.indirect_draw_cmds");
#endif

    // Gate uploads on non-empty sources: when drawCount==0 the carrier
    // mesh path keeps the buffers at their element-size minimums (already
    // grown by growBuf above) and skips the upload. Procedural consumers
    // never read these slots; mesh consumers don't draw, so contents are
    // irrelevant.
    if(!acc.perDraws.empty())
      res.uploadStaticBuffer(
          m_mdi.per_draws, 0,
          (int64_t)acc.perDraws.size() * sizeof(PerDrawGPU),
          acc.perDraws.data());
    if(!acc.indirectCmds.empty())
      res.uploadStaticBuffer(
          m_mdi.indirect_draw_cmds, 0,
          (int64_t)acc.indirectCmds.size() * sizeof(Acc::IndirectCmd),
          acc.indirectCmds.data());
    if(pdbBytes > 0)
      res.uploadStaticBuffer(
          m_mdi.per_draw_bounds, 0, pdbBytes, acc.perDrawBounds.data());

    // Seed the fast-path diff-upload mirror from the ACTUALLY-EMITTED set
    // (acc.perDraws / acc.perDrawBounds), restricted to the fs.draws prefix
    // (instance-group entries are never compared on the fast path — it's
    // gated on fs.instances.empty()). Seeding from `freshPerDraws` (filtered
    // only by vertices>0) would diverge whenever emitDraw skipped a draw,
    // making diffUpload write a neighbour's model matrix into the wrong slot.
    m_cachedPerDraws.assign(
        acc.perDraws.begin(),
        acc.perDraws.begin() + (std::ptrdiff_t)meshDrawCount);
    m_cachedPerDrawBounds.assign(
        acc.perDrawBounds.begin(),
        acc.perDrawBounds.begin() + (std::ptrdiff_t)meshDrawCount);

    // ── Per-instance concat buffers (unified MDI) ───────────────────────
    //
    // Three parallel arrays sized to slot_cursor:
    //   - draw_ids[k]      : cmd index of the cmd that owns slot k
    //   - translations[k]  : vec4 (xyz used) — identity for regular cmd
    //                        slots, GPU-copied per-particle position for
    //                        instance group slots
    //   - colors[k]        : vec4 — identity (1,1,1,1) for regular cmd
    //                        slots, GPU-copied per-instance color for
    //                        groups
    //
    // Layout invariant: every regular fs.draws cmd at acc index i lands
    // at slot i (instanceCount=1). Instance groups follow contiguously
    // (slot >= acc.indirectCmds.size() - fs.instances.size() in general,
    // but the bookkeeping is captured per-group in instanceRecords). The
    // shader reads `draw_id` as a per-instance VERTEX_INPUT and indexes
    // per_draws[draw_id] — works on both indirect and CPU-fallback paths
    // because firstInstance is the only state needed (no gl_DrawID
    // dependency).
    if(slot_cursor > 0)
    {
      const int64_t drawIdsBytes      = (int64_t)slot_cursor * 4;
      const int64_t translationsBytes = (int64_t)slot_cursor * 16;
      const int64_t colorsBytes       = (int64_t)slot_cursor * 16;

      // m_instDrawIds: paired with diff-upload via m_cachedInstDrawIds
      // at line 2544. On realloc we MUST clear the mirror — this is the
      // root cause of the "instances disappear at p2-of instance count"
      // bug (4→5, 8→9, 16→17 …). For an Instancer with one prototype
      // every slot's draw_id is the same value (the cmd_index, usually
      // 0), so cached and fresh are byte-identical for the prefix and
      // diffUpload's equal-runs branch (line 783) skips them — leaving
      // the new buffer's prefix as uninitialised driver memory. The
      // basic-unlit / classic_pbr vertex shader then OOBs on
      // per_draws[draw_id] for every "garbage" instance. Translations /
      // colors are immune (full GPU copy at lines 2606+), so they don't
      // need the clear, but cleaning the diff-upload one is mandatory.
      if(growBuf(renderer, res,m_instDrawIds, m_instDrawIdsCap, drawIdsBytes,
                 UF(QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer),
                 "ScenePreprocessor::inst.draw_ids"))
        m_cachedInstDrawIds.clear();
      growBuf(renderer, res,m_instTranslations, m_instTranslationsCap,
              translationsBytes,
              UF(QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer),
              "ScenePreprocessor::inst.translations");
      growBuf(renderer, res,m_instColors, m_instColorsCap, colorsBytes,
              UF(QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer),
              "ScenePreprocessor::inst.colors");

      // Build the full draw_ids vector. For a regular fs.draws cmd at
      // acc index i: draw_ids[i] = i. For instance group records: the
      // group's slot range gets cmd_index repeated `count` times.
      // Diff-uploaded via the m_cachedInstDrawIds mirror so steady-state
      // frames touch zero bytes when the topology is unchanged.
      std::vector<uint32_t> fresh_draw_ids(slot_cursor, 0u);
      // Regular cmds: each occupies one slot at acc index = slot index.
      const std::size_t n_regular_cmds
          = acc.indirectCmds.size() - instanceRecords.size();
      for(std::size_t i = 0; i < n_regular_cmds; ++i)
        fresh_draw_ids[i] = (uint32_t)i;
      for(const auto& rec : instanceRecords)
      {
        for(uint32_t k = 0; k < rec.count; ++k)
          fresh_draw_ids[rec.slot_base + k] = rec.cmd_index;
      }
      diffUpload(res, m_instDrawIds, m_cachedInstDrawIds, fresh_draw_ids);

      // Regular-slot identity values for translations + colors. Instance
      // group slots (offset >= n_regular_cmds * 16) are filled by the
      // GPU copies below — uploadStaticBuffer here covers ONLY the
      // regular range so we don't stomp the GPU-copied data. Instance
      // group slot ranges that overlap stale content from a previous
      // frame are overwritten by the per-frame GPU copy.
      if(n_regular_cmds > 0)
      {
        std::vector<float> regular_translations(n_regular_cmds * 4, 0.f);
        std::vector<float> regular_colors(n_regular_cmds * 4, 1.f);
        res.uploadStaticBuffer(
            m_instTranslations, 0,
            (quint32)(n_regular_cmds * 16),
            regular_translations.data());
        res.uploadStaticBuffer(
            m_instColors, 0,
            (quint32)(n_regular_cmds * 16),
            regular_colors.data());
      }

      // Queue GPU copies for instance groups. Each record copies
      // `count` instances from the upstream Instancer's source buffer
      // into the concat array at `slot_base * stride` bytes. The
      // record's src_translation_offset is biased per source format so
      // each strided slice lands on the actual translation bytes:
      //  - translation (vec4):  bytes [0..15]  = (x, y, z, w).
      //  - trs (vec3 T + ...):  bytes [0..15]  = T + 4 leading bytes
      //    of R; the shader binds vec3 from offset 0 so stray R bytes
      //    are never sampled.
      //  - mat4 (col-major):    bytes [48..63] = column 3 = (Tx, Ty, Tz, 1).
      auto queueInstanceCopy = [&](
          QRhiBuffer* src, uint32_t srcOffset, uint32_t srcStride,
          QRhiBuffer* dst, uint32_t dstOffset, uint32_t count,
          uint32_t elemSize)
      {
        if(!src || !dst || count == 0)
          return;
        PendingGpuCopy op;
        op.attr = MdiAttr::Positions;  // unused when dst is set explicitly
        op.src = src;
        op.dst = dst;
        op.src_offset = (int)srcOffset;
        op.dst_offset = (int)dstOffset;
        op.vertex_count = (int)count;
        op.src_stride = (int)srcStride;
        op.element_size = (int)elemSize;
        op.size = (op.src_stride == 0 || op.src_stride == op.element_size)
                      ? op.vertex_count * op.element_size
                      : op.element_size;
        m_pendingGpuCopies.push_back(op);
      };
      for(const auto& rec : instanceRecords)
      {
        // Translation: copy 12 bytes per instance into the leading
        // bytes of each vec4-stride slot. The slot's trailing 4 bytes
        // remain garbage / leftover (identity uploads only cover the
        // regular range above) — the shader binds vec3 from offset 0
        // so the trailing pad is never sampled.
        if(rec.src_translations)
        {
          queueInstanceCopy(
              rec.src_translations, rec.src_translation_offset,
              rec.src_translation_stride,
              m_instTranslations, rec.slot_base * 16, rec.count,
              /*elemSize=*/16);
        }
        if(rec.src_colors)
        {
          queueInstanceCopy(
              rec.src_colors, rec.src_color_offset, /*srcStride=*/16,
              m_instColors, rec.slot_base * 16, rec.count,
              /*elemSize=*/16);
        }
      }
    }

    // Build the output geometry referencing the arena stream buffers
    // (pointer-stable across frames and scene churn).
    ossia::geometry g;
    auto wrapGpu = [](QRhiBuffer* b, int64_t size) {
      ossia::geometry::gpu_buffer gb;
      gb.handle = b;
      gb.byte_size = size;
      return ossia::geometry::buffer{.data = gb, .dirty = true};
    };

    // The "byte_size" on each gpu_buffer is the binding extent
    // downstream consumers use when constructing their MeshBuffer
    // BufferViews. Using the arena's full capacity (stable across
    // frames) keeps downstream pointer identity + extent identical
    // frame-over-frame — the per-draw `baseVertex` in
    // indirect_draw_cmds addresses into this range.
    const int64_t posCapBytes
        = (int64_t)GpuResourceRegistry::kMeshCapBytes[(int)Stream::Positions];
    const int64_t nrmCapBytes
        = (int64_t)GpuResourceRegistry::kMeshCapBytes[(int)Stream::Normals];
    const int64_t uvCapBytes
        = (int64_t)GpuResourceRegistry::kMeshCapBytes[(int)Stream::Texcoords];
    const int64_t tanCapBytes
        = (int64_t)GpuResourceRegistry::kMeshCapBytes[(int)Stream::Tangents];
    const int64_t colCapBytes
        = (int64_t)GpuResourceRegistry::kMeshCapBytes[(int)Stream::Colors];
    const int64_t uv1CapBytes
        = (int64_t)GpuResourceRegistry::kMeshCapBytes[(int)Stream::Texcoords1];
    const int64_t idxCapBytes
        = (int64_t)GpuResourceRegistry::kMeshCapBytes[(int)Stream::Indices];

    // Buffer slot order is wired through to the index-buffer slot
    // below — keep buffers 0..5 as the six vertex streams, slot 6 as
    // the index buffer. Adding/reordering here REQUIRES updating
    // g.index.buffer at the end of this block.
    g.buffers.push_back(wrapGpu(
        m_registry->meshStreamBuffer(Stream::Positions), posCapBytes));
    g.buffers.push_back(wrapGpu(
        m_registry->meshStreamBuffer(Stream::Normals), nrmCapBytes));
    g.buffers.push_back(wrapGpu(
        m_registry->meshStreamBuffer(Stream::Texcoords), uvCapBytes));
    g.buffers.push_back(wrapGpu(
        m_registry->meshStreamBuffer(Stream::Tangents), tanCapBytes));
    g.buffers.push_back(wrapGpu(
        m_registry->meshStreamBuffer(Stream::Colors), colCapBytes));
    g.buffers.push_back(wrapGpu(
        m_registry->meshStreamBuffer(Stream::Texcoords1), uv1CapBytes));
    g.buffers.push_back(wrapGpu(
        m_registry->meshStreamBuffer(Stream::Indices), idxCapBytes));

    // MDI uses vec4 stride (16 B) for position and normal even though the
    // shader binding format is float3. Vulkan reads the first 12 bytes of
    // each 16-byte slot for vec3, so the last 4 bytes are unused padding.
    // Why: GPU-resident vertex sources (compute-shader outputs) naturally
    // emit vec3 inside a 16-byte-aligned slot due to std430/std140 layout
    // rules. Matching MDI stride lets us turn what would be a per-vertex
    // strided copyBuffer loop (O(N) vkCmdCopyBuffer regions per frame)
    // into a single tight blit. Cost: 33 % extra memory for pos/nrm only.
    ossia::geometry::binding bPos{}; bPos.byte_stride = 16; bPos.classification = ossia::geometry::binding::per_vertex;
    ossia::geometry::binding bNrm{}; bNrm.byte_stride = 16; bNrm.classification = ossia::geometry::binding::per_vertex;
    ossia::geometry::binding bUv{};  bUv.byte_stride  = 8;  bUv.classification  = ossia::geometry::binding::per_vertex;
    ossia::geometry::binding bTan{}; bTan.byte_stride = 16; bTan.classification = ossia::geometry::binding::per_vertex;
    ossia::geometry::binding bCol{}; bCol.byte_stride = 16; bCol.classification = ossia::geometry::binding::per_vertex;
    ossia::geometry::binding bUv1{}; bUv1.byte_stride = 8;  bUv1.classification = ossia::geometry::binding::per_vertex;
    g.bindings.push_back(bPos);
    g.bindings.push_back(bNrm);
    g.bindings.push_back(bUv);
    g.bindings.push_back(bTan);
    g.bindings.push_back(bCol);
    g.bindings.push_back(bUv1);

    // `input` is both the type and the vector member on geometry; use the
    // elaborated `struct` tag to disambiguate in this scope.
    using GeomInput = struct ossia::geometry::input;
    g.input.push_back(GeomInput{.buffer = 0, .byte_offset = 0});
    g.input.push_back(GeomInput{.buffer = 1, .byte_offset = 0});
    g.input.push_back(GeomInput{.buffer = 2, .byte_offset = 0});
    g.input.push_back(GeomInput{.buffer = 3, .byte_offset = 0});
    g.input.push_back(GeomInput{.buffer = 4, .byte_offset = 0});
    g.input.push_back(GeomInput{.buffer = 5, .byte_offset = 0});

    auto pushAttr = [&](ossia::attribute_semantic sem, int binding,
                        decltype(ossia::geometry::attribute::format) fmt) {
      ossia::geometry::attribute a{};
      a.binding = binding;
      a.byte_offset = 0;
      a.format = fmt;
      a.semantic = sem;
      g.attributes.push_back(a);
    };
    pushAttr(ossia::attribute_semantic::position,  0, ossia::geometry::attribute::float3);
    pushAttr(ossia::attribute_semantic::normal,    1, ossia::geometry::attribute::float3);
    pushAttr(ossia::attribute_semantic::texcoord0, 2, ossia::geometry::attribute::float2);
    pushAttr(ossia::attribute_semantic::tangent,   3, ossia::geometry::attribute::float4);
    pushAttr(ossia::attribute_semantic::color0,    4, ossia::geometry::attribute::float4);
    pushAttr(ossia::attribute_semantic::texcoord1, 5, ossia::geometry::attribute::float2);

    // ── Per-instance vertex bindings (unified MDI) ──────────────────────
    //
    // Three PerInstance step_rate=1 bindings carry the unified-MDI
    // per-instance state. Each indirect cmd (regular or instance group)
    // sets `firstInstance = its own slot offset` so these bindings
    // address the right slice of each concat buffer on both the
    // indirect path and the CPU-fallback drawIndexed loop.
    //
    // Buffer slot order in `g.buffers`:
    //   0..5  per-vertex streams (pos / nrm / uv0 / tan / col / uv1)
    //   6     index buffer
    //   7     inst_translations (vec4 stride 16)
    //   8     inst_colors (vec4 stride 16)
    //   9     inst_draw_ids (uint stride 4)
    // Adding more slots HERE shifts every subsequent aux's buf index;
    // the post-section building auxiliaries computes its base via
    // `baseBuf = (int)g.buffers.size()` so it doesn't need changing.
    if(slot_cursor > 0 && m_instTranslations && m_instColors && m_instDrawIds)
    {
      // Index buffer must come before per-instance buffers since
      // g.index.buffer is hard-coded to slot 6 below; per-instance
      // buffers occupy slots 7, 8, 9.
      g.buffers.push_back(wrapGpu(
          m_instTranslations, (int64_t)slot_cursor * 16));
      g.buffers.push_back(wrapGpu(
          m_instColors, (int64_t)slot_cursor * 16));
      g.buffers.push_back(wrapGpu(
          m_instDrawIds, (int64_t)slot_cursor * 4));

      ossia::geometry::binding bInstT{};
      bInstT.byte_stride = 16;
      bInstT.classification = ossia::geometry::binding::per_instance;
      bInstT.step_rate = 1;
      const int instTBindIdx = (int)g.bindings.size();
      g.bindings.push_back(bInstT);

      ossia::geometry::binding bInstC{};
      bInstC.byte_stride = 16;
      bInstC.classification = ossia::geometry::binding::per_instance;
      bInstC.step_rate = 1;
      const int instCBindIdx = (int)g.bindings.size();
      g.bindings.push_back(bInstC);

      ossia::geometry::binding bInstD{};
      bInstD.byte_stride = 4;
      bInstD.classification = ossia::geometry::binding::per_instance;
      bInstD.step_rate = 1;
      const int instDBindIdx = (int)g.bindings.size();
      g.bindings.push_back(bInstD);

      g.input.push_back(GeomInput{.buffer = 7, .byte_offset = 0});
      g.input.push_back(GeomInput{.buffer = 8, .byte_offset = 0});
      g.input.push_back(GeomInput{.buffer = 9, .byte_offset = 0});

      // Per-instance attributes. Translation reuses the existing
      // `translation` semantic (no per-vertex `translation` ever exists,
      // so no collision). Color uses the dedicated `instance_color0`
      // semantic added to libossia for unified MDI to avoid the
      // per-vertex / per-instance `color0` collision in
      // findGeometryAttribute. draw_id uses `instance_draw_id`
      // (uint-typed; required by every shader using per_draws[] in
      // the unified-MDI path).
      pushAttr(ossia::attribute_semantic::translation,
               instTBindIdx, ossia::geometry::attribute::float3);
      pushAttr(ossia::attribute_semantic::instance_color0,
               instCBindIdx, ossia::geometry::attribute::float4);
      pushAttr(ossia::attribute_semantic::instance_draw_id,
               instDBindIdx, ossia::geometry::attribute::uint1);
    }

    g.vertices  = (int)m_mdi.totalVertices;
    g.indices   = (int)m_mdi.totalIndices;
    g.instances = 1;
    g.topology  = ossia::geometry::triangles;
    // glTF doubleSided: pipeline-side culling is OFF for the MDI
    // batch. Per-fragment culling is shader-side, driven by each
    // material's `feature_mask`:
    //   - single-sided (no `double_sided` bit): shader discards
    //     `!gl_FrontFacing` fragments → matches CULL_BACK behaviour.
    //   - double-sided: shader keeps both sides and flips the surface
    //     normal for back-facing fragments so lighting works on both.
    // Splitting the MDI batch by cull mode would multiply the draw
    // count and lose much of the indirect-draw benefit; per-fragment
    // gating is the simpler trade.
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;

    g.index.buffer = 6;  // Slot order: pos=0, nrm=1, uv=2, tan=3, col=4, uv1=5, idx=6.
    g.index.byte_offset = 0;
    g.index.format = decltype(g.index)::uint32;

    // filter_tag / filter_material_index are per-geometry metadata
    // used by mesh-level filters (FlattenedSceneFilterNode). The
    // preprocessor emits ONE geometry per MDI batch spanning many
    // materials, so there's no single value that would be meaningful
    // here — we stamp 0 so those filters either drop or keep the
    // whole batch. Per-draw material / tag filtering belongs to a
    // compute-shader filter that consumes indirect_draw_cmds +
    // per_draws (CSF-based, see docs on scene_filter_* presets).
    g.filter_tag = 0;
    g.filter_material_index = 0;

    // Attach scene-wide auxiliaries. Shaders pick these up by NAME via
    // try_bind_from_geometry, so there's no need for downstream nodes to
    // wire every SSBO/UBO manually — the geometry cable already carries
    // scene lights / materials / per-draws / indirect / counts / camera
    // / env. The names here MUST match the shader's `INPUTS[].NAME`.
    const int baseBuf = (int)g.buffers.size();
    // scene_lights → RawLight arena directly.
    // Every classic_pbr_*.frag's Light struct now matches the arena
    // layout and the light loop reads
    // scene_lights.entries[scene_light_indices.data[i]], composing
    // world-space direction from world_transforms[transform_slot].
    {
      auto* lightArena
          = renderer.registry().buffer(GpuResourceRegistry::Arena::RawLight);
      const int64_t lightArenaBytes
          = (int64_t)renderer.registry().arenaSlotStride(
                GpuResourceRegistry::Arena::RawLight)
            * (int64_t)renderer.registry().arenaSlotCount(
                GpuResourceRegistry::Arena::RawLight);
      g.buffers.push_back(wrapGpu(lightArena, lightArenaBytes));
    }
    // scene_materials binding points at the Material arena directly.
    // Shader indexes entries[material_index] where material_index is
    // the arena slot index (stamped in PerDrawGPU above) and the SSBO
    // stride matches sizeof(MaterialGPU) = 80B. Eliminates the
    // per-frame CPU-side repack + upload that m_materialsBuffer used
    // to carry.
    {
      auto* matArena
          = renderer.registry().buffer(GpuResourceRegistry::Arena::Material);
      const int64_t matArenaBytes
          = (int64_t)renderer.registry().arenaSlotStride(
                GpuResourceRegistry::Arena::Material)
            * (int64_t)renderer.registry().arenaSlotCount(
                GpuResourceRegistry::Arena::Material);
      g.buffers.push_back(wrapGpu(matArena, matArenaBytes));
    }
    g.buffers.push_back(wrapGpu(m_materialsExtBuffer, m_materialsExtCap));
    g.buffers.push_back(wrapGpu(m_mdi.per_draws,          pdBytes));
    g.buffers.push_back(wrapGpu(m_mdi.indirect_draw_cmds, icBytes));
    g.buffers.push_back(wrapGpu(m_sceneCountsBuffer, sizeof(SceneCountsUBO)));
    // Only bind the ACTIVE camera slot (first 240 bytes) — shaders declare
    // `uniform camera_t camera` as a single entry, not an array. Slot 0 is
    // guaranteed to be the active camera by packAndUploadCameras.
    g.buffers.push_back(wrapGpu(m_camerasBuffer, sizeof(CameraUBOData)));
    g.buffers.push_back(wrapGpu(m_camerasPrevBuffer, sizeof(CameraUBOData)));
    // Env UBO: bind a PREPROCESSOR-owned slot, not any single producer's
    // slot. With multi-producer env composition the merged
    // scene_environment is built field-by-field by merge_scenes from
    // every contributing EnvironmentLoader / CubemapLoader — no single
    // producer's slot holds the merged result. The preprocessor packs
    // the merged CPU-side env into m_envSlot here and consumers bind
    // that offset.
    m_env_aux_offset = renderer.registry().slotOffset(m_envSlot);
    g.buffers.push_back(wrapGpu(
        renderer.registry().buffer(GpuResourceRegistry::Arena::Env),
        sizeof(EnvParamsUBO)));
    // World transforms — arena-slot-indexed. Consumer
    // shaders read world_transforms.data[slot_index] for any light /
    // particle / compute pass that needs slot-addressable world-space
    // composition. Preprocessor-private so multi-filter pipelines don't
    // stomp each other.
    g.buffers.push_back(wrapGpu(
        m_worldTransformsBuffer, m_worldTransformsCap));
    // Previous-frame snapshot of the same layout; consumer shaders
    // declare an AUXILIARY / storage input named `world_transforms_prev`
    // to read it for motion vectors, TAA, reprojection, etc.
    g.buffers.push_back(wrapGpu(
        m_worldTransformsPrevBuffer, m_worldTransformsCap));
    // scene_light_indices — compact list of RawLight arena slot indices
    // for the scene's live lights. Shader iterates
    // 0..scene_counts.light_count, reads
    // scene_lights.entries[scene_light_indices.data[i]].
    g.buffers.push_back(wrapGpu(
        m_lightIndicesBuffer, m_lightIndicesCap));

    {
      const int64_t lightArenaBytes
          = (int64_t)renderer.registry().arenaSlotStride(
                GpuResourceRegistry::Arena::RawLight)
            * (int64_t)renderer.registry().arenaSlotCount(
                GpuResourceRegistry::Arena::RawLight);
      g.auxiliary.push_back({
          .name = "scene_lights", .buffer = baseBuf,
          .byte_offset = 0,
          .byte_size = lightArenaBytes});
    }
    {
      const int64_t matArenaBytes
          = (int64_t)renderer.registry().arenaSlotStride(
                GpuResourceRegistry::Arena::Material)
            * (int64_t)renderer.registry().arenaSlotCount(
                GpuResourceRegistry::Arena::Material);
      g.auxiliary.push_back({
          .name = "scene_materials", .buffer = baseBuf + 1,
          .byte_offset = 0,
          .byte_size = matArenaBytes});
    }
    // Parallel to scene_materials — same element count, same indexing.
    // OpenPBR-grade shaders bind this as a second SSBO and use the same
    // material_index to read the extension struct.
    // byte_size = full buffer capacity. The buffer is sized in update()
    // to (max_arena_slot + 1) * sizeof(MaterialExtensionsGPU) — see the
    // arenaSlotEntries computation there. The shader indexes by
    // pd.material_index (arena slot), so the binding extent must cover
    // the full arena range.
    g.auxiliary.push_back({
        .name = "scene_materials_ext", .buffer = baseBuf + 2,
        .byte_offset = 0,
        .byte_size = m_materialsExtCap});
    g.auxiliary.push_back({
        .name = "per_draws", .buffer = baseBuf + 3,
        .byte_offset = 0, .byte_size = pdBytes});
    g.auxiliary.push_back({
        .name = "indirect_draw_cmds", .buffer = baseBuf + 4,
        .byte_offset = 0, .byte_size = icBytes});
    g.auxiliary.push_back({
        .name = "scene_counts", .buffer = baseBuf + 5,
        .byte_offset = 0, .byte_size = (int64_t)sizeof(SceneCountsUBO)});
    g.auxiliary.push_back({
        .name = "camera", .buffer = baseBuf + 6,
        .byte_offset = 0, .byte_size = (int64_t)sizeof(CameraUBOData)});
    g.auxiliary.push_back({
        .name = "camera_prev", .buffer = baseBuf + 7,
        .byte_offset = 0, .byte_size = (int64_t)sizeof(CameraUBOData)});
    g.auxiliary.push_back({
        .name = "env", .buffer = baseBuf + 8,
        .byte_offset = (int64_t)m_env_aux_offset,
        .byte_size = (int64_t)sizeof(EnvParamsUBO)});
    g.auxiliary.push_back({
        .name = "world_transforms", .buffer = baseBuf + 9,
        .byte_offset = 0,
        .byte_size = m_worldTransformsCap});
    // Previous-frame snapshot for motion-vector / TAA / reprojection
    // shaders. Snapshot is produced in runInitialPasses via a single
    // GPU-side copyBuffer; the per-slot writes for the same frame
    // are deferred from update() into the next resource-update batch
    // so the copy reads the still-frame-N-1 contents of current.
    g.auxiliary.push_back({
        .name = "world_transforms_prev", .buffer = baseBuf + 10,
        .byte_offset = 0,
        .byte_size = m_worldTransformsCap});
    g.auxiliary.push_back({
        .name = "scene_light_indices", .buffer = baseBuf + 11,
        .byte_offset = 0,
        .byte_size = m_lightIndicesCap});

    // KHR_texture_transform: per-material per-channel UV transforms.
    // Parallel to scene_materials, indexed by material_index. Identity
    // transforms for materials without the extension — zero shader cost.
    {
      const int buf_idx = (int)g.buffers.size();
      g.buffers.push_back(wrapGpu(
          m_materialUVTransformsBuffer, m_materialUVTransformsCap));
      g.auxiliary.push_back({
          .name = "scene_material_uv_xforms", .buffer = buf_idx,
          .byte_offset = 0,
          .byte_size = m_materialUVTransformsCap});
    }

    // per_draw_bounds — sidecar to per_draws, one local-space AABB per
    // draw (std430 2×vec4 = 32 B). Consumer: GPU culling shaders
    // (scene_filter_aabb_cull.csf and the future HiZ variant) read this
    // together with per_draws[i].model to frustum-test each draw and
    // rewrite indirect_draw_cmds[i] with indexCount=0 when culled.
    {
      const int buf_idx = (int)g.buffers.size();
      g.buffers.push_back(wrapGpu(m_mdi.per_draw_bounds, pdbBytes));
      g.auxiliary.push_back({
          .name = "per_draw_bounds", .buffer = buf_idx,
          .byte_offset = 0, .byte_size = pdbBytes});
    }

    // shadow_cascades UBO — 544 B, std140. Consumer: classic_pbr_shadowed
    // PCF cascade pick + light_view_proj sampling, and the shadow-pass
    // depth-only shader's light_view_proj array. Populated from
    // scene_state.shadow_cascades (Threedim::ShadowCascadeSetup). Always
    // published — when no upstream authored cascades, cascade_count=0
    // signals consumers to skip shadow sampling (the shader-side guard
    // already handles this).
    if(m_shadowCascadesBuffer)
    {
      const int buf_idx = (int)g.buffers.size();
      g.buffers.push_back(wrapGpu(
          m_shadowCascadesBuffer, (int64_t)sizeof(ShadowCascadesUBO)));
      g.auxiliary.push_back({
          .name = "shadow_cascades", .buffer = buf_idx,
          .byte_offset = 0,
          .byte_size = (int64_t)sizeof(ShadowCascadesUBO)});
    }

    // Attach per-channel material texture arrays + skybox as auxiliary
    // textures. Consumer shaders (classic_pbr_textured / classic_pbr_ibl /
    // classic_pbr_full) pick them up by NAME through the same
    // try_bind_texture_from_geometry mechanism as the buffer auxes above —
    // no manual cable required. Null handles are filtered out so a shader
    // missing a given channel falls back to its own sampler (emptyTexture).
    appendTextureAuxes(g);

    // Mid-pipeline aux injection from InjectBuffer / InjectTexture nodes
    // upstream. Name collisions with preprocessor-owned auxes are resolved
    // last-wins: we append these AFTER the preprocessor's own entries, and
    // consumer-side find_auxiliary / find_auxiliary_texture return the
    // LAST match when we pre-remove colliding earlier entries below.
    //
    // Buffer injections: wrap each handle as a geometry-buffer slot, add
    // an auxiliary_buffer entry pointing at it.
    if(this->scene.state)
    {
      for(const auto& ib : this->scene.state->inject_buffers)
      {
        if(!ib.native_handle || ib.name.empty())
          continue;
        // Remove any earlier entry with the same name so the injection
        // wins (consumer find_auxiliary returns first-match; easier to
        // maintain "last-wins" semantics by purging the earlier one).
        auto& aux_list = g.auxiliary;
        aux_list.erase(
            std::remove_if(
                aux_list.begin(), aux_list.end(),
                [&](const ossia::geometry::auxiliary_buffer& a) {
                  return a.name == ib.name;
                }),
            aux_list.end());
        const int buf_idx = (int)g.buffers.size();
        g.buffers.push_back(
            wrapGpu(static_cast<QRhiBuffer*>(ib.native_handle), ib.byte_size));
        g.auxiliary.push_back(
            {.name = ib.name,
             .buffer = buf_idx,
             .byte_offset = 0,
             .byte_size = ib.byte_size});
      }
      for(const auto& it : this->scene.state->inject_textures)
      {
        if(!it.native_handle || it.name.empty())
          continue;
        auto& tex_list = g.auxiliary_textures;
        tex_list.erase(
            std::remove_if(
                tex_list.begin(), tex_list.end(),
                [&](const ossia::geometry::auxiliary_texture& a) {
                  return a.name == it.name;
                }),
            tex_list.end());
        g.auxiliary_textures.push_back(
            {.name = it.name, .native_handle = it.native_handle});
      }
    }

    // Use the existing indirect_count slot for the draw count — renderers
    // that support drawIndexedIndirect pick it up automatically.
    //
    // drawCount==0 carrier-mesh path: leave indirect_count.handle null
    // so CustomMesh::drawSingleMesh skips its indirect-draw branch
    // (which would otherwise issue cb.drawIndirect against a buffer
    // whose contents weren't uploaded this frame, yielding the
    // UINT32_MAX-firstIndex Vulkan validation error). The carrier still
    // gets pushed onto m_outputSpec.meshes as a pure aux carrier for
    // procedural-only consumers (skybox, fullscreen effects); they read
    // the auxiliary list and don't issue an indirect draw themselves.
    // Mesh consumers fall through to `cb.draw(0, 0)` — a no-op.
    ossia::geometry::gpu_buffer ic_count;
    if(!acc.indirectCmds.empty())
    {
      ic_count.handle = m_mdi.indirect_draw_cmds;
      ic_count.byte_size = icBytes;
    }
    g.indirect_count = ic_count;

    // CPU-side copy of indirect draw commands for the Qt < 6.12 fallback
    // path. CustomMesh::draw iterates these and issues per-command
    // drawIndexed calls with the correct firstInstance / baseVertex.
    g.cpu_draw_commands.reserve(acc.indirectCmds.size());
    for(const auto& cmd : acc.indirectCmds)
    {
      g.cpu_draw_commands.push_back({
          .index_or_vertex_count = cmd.indexCount,
          .instance_count = cmd.instanceCount,
          .first_index_or_vertex = cmd.firstIndex,
          .base_vertex = cmd.baseVertex,
          .first_instance = cmd.baseInstance});
    }

    auto meshes = std::make_shared<ossia::mesh_list>();
    meshes->meshes.push_back(std::move(g));
    meshes->dirty_index
        = (m_outputSpec.meshes ? m_outputSpec.meshes->dirty_index : 0) + 1;

    m_outputSpec.meshes = std::move(meshes);
    if(!m_outputSpec.filters)
      m_outputSpec.filters = std::make_shared<ossia::geometry_filter_list>();
  }


  // Decode a texture_source to an RGBA8888 QImage. Single-texture-point of
  // decode so the rebuild code below can dedupe upstream of JPEG decoding.
  //
  // When `src.content_hash != 0` and an AssetTable is
  // available, peek the cache first. On hit: skip decode, return the
  // cached QImage directly. On miss: decode, stage into the cache so
  // future RenderLists (other outputs, reloads within the session) hit
  // without re-decoding. Zero-hash sources (legacy parsers that don't
  // populate the hash) always take the decode path.
  static QImage decodeTextureSource(
      const ossia::texture_source& src, Gfx::AssetTable* cache)
  {
    if(cache && src.content_hash != 0)
    {
      if(auto asset = cache->peek(src.content_hash); asset && !asset->image.isNull())
        return asset->image;
    }

    std::optional<DecodedImage> decoded;
    if(src.embedded_data && !src.embedded_data->empty())
    {
      QByteArray bytes(
          reinterpret_cast<const char*>(src.embedded_data->data()),
          (qsizetype)src.embedded_data->size());
      decoded = decodeImageFromMemory(
          bytes, QString::fromStdString(src.mime_type));
    }
    else if(!src.file_path.empty())
    {
      decoded = decodeImageFromPath(QString::fromStdString(src.file_path));
    }
    if(decoded && !decoded->image.isNull())
    {
      // Stage into the cross-output decode cache so the next
      // RenderList / reload hits without re-decoding. Stage is
      // idempotent — same hash re-staged is a no-op.
      if(cache && src.content_hash != 0)
        cache->stage(src.content_hash, decoded->image);
      return decoded->image;
    }
    QImage fallback(1, 1, QImage::Format_RGBA8888);
    fallback.fill(Qt::white);
    return fallback;
  }

  // Build a content fingerprint of the current materials list — keyed on
  // material_component::stable_id rather than the raw pointer. Stable
  // across producer rebuilds (the producer re-emits a fresh shared_ptr
  // with the same id) AND across merge_scenes contributor reshuffles.
  // Falls back to the pointer bits when stable_id is zero so un-stamped
  // legacy producers still work (just with less-stable semantics).
  void computeMaterialsFingerprint(std::vector<uint64_t>& out) const
  {
    out.clear();
    if(!this->scene.state || !this->scene.state->materials)
      return;
    const auto& mats = *this->scene.state->materials;
    out.reserve(mats.size());
    for(const auto& m : mats)
    {
      if(!m)
      {
        out.push_back(0);
        continue;
      }
      out.push_back(
          m->stable_id != 0
              ? m->stable_id
              : reinterpret_cast<uint64_t>(m.get()));
    }
  }

  // (Re)allocate a material-texture channel's array, deduping by
  // texture_source pointer so N materials that share one image upload
  // ONE layer, not N. Patches fs.materials[i].textureRefs[ch] with the
  // packed layer ref for material i.
  //
  // Call sequence in update():
  //   flattenScene → fs.materials      ← un-patched, all textureRefs=NONE
  //   computeMaterialsFingerprint(fp)  ← snapshot element ptrs
  //   rebuildChannel(ch, fp, fs, …)    ← dedupes + patches textureRefs[ch]
  //   diffUpload / uploadStaticBuffer of scene_materials SSBO
  //
  // `sameMaterialsContent` is the result of comparing `fp` to
  // `m_cachedMaterialsFingerprint`, computed once per update() and passed
  // in so the ChannelCount rebuildChannel calls each frame don't each
  // re-walk the list.
  //
  // Returns true if the channel's QRhiTexture* was (re)allocated —
  // caller uses this to trigger downstream SRB rebinds.
  // Walk materials and assign dynamic-slot indices for texture_refs that
  // carry a GPU handle without a source. Rebuilt every frame because the
  // upstream QRhiTexture* can swap without the material_component pointer
  // changing (e.g., video-texture resized mid-stream). Cheap: O(n_mats),
  // no uploads. Materials past the slot cap recycle the LRU-oldest slot
  // (per resolveDynamicSlot's eviction path); the corresponding shader
  // sampler now points at the new texture rather than tex_ref_none.
  void rebuildDynamicSlots(MaterialChannel ch)
  {
    // Dynamic slot maps persist across the registry's lifetime — they
    // are NOT cleared per-frame (cleared only in GpuResourceRegistry
    // init()/destroy()). resolveDynamicSlot is idempotent on the same
    // QRhiTexture* handle, so re-registering during this per-channel
    // pass is a no-op for handles that haven't changed and refreshes
    // the LRU last-use stamp on hit. Producers (PBRMesh,
    // MaterialOverride) calling resolveDynamicSlot before this pass
    // agree on the same slot index for the same handle.
    if(!this->scene.state || !this->scene.state->materials || !m_registry)
      return;

    // Resolve a single dynamic-handle texture_ref into the channel's
    // dynamic slot map. Static refs (with a CPU-side `source`) and
    // empty refs short-circuit out — only refs carrying a runtime GPU
    // handle land here. Idempotent for repeated handle / multi-channel
    // routing.
    const auto resolve_dyn = [this, ch](const ossia::texture_ref& tref) {
      if(tref.source)
        return;
      if(!tref.texture.valid())
        return;
      m_registry->resolveDynamicSlot(toTexChannel(ch), tref.texture.native_handle);
    };

    for(const auto& m : *this->scene.state->materials)
    {
      if(!m)
        continue;
      // Main channel ref (the existing path).
      if(const auto* tref = channelRef(ch, *m); tref)
        resolve_dyn(*tref);
      // Ext-table refs whose pool matches this channel.
      for(const auto& slot : kExtTextureSlots)
        if(slot.channel == ch)
          resolve_dyn(slot.accessor(*m));
    }
  }

  bool rebuildChannel(
      MaterialChannel ch, bool sameMaterialsContent, RenderList& renderer,
      QRhiResourceUpdateBatch& res, FlatScene& fs)
  {
    if(!m_registry)
      return false;
    auto& rhi = *renderer.state.rhi;
    auto& channel = texChannel(ch);

    const auto matsPtr
        = this->scene.state ? this->scene.state->materials : nullptr;

    // Dynamic slots refresh every frame regardless of sameMaterialsContent:
    // runtime handles can swap without the outer material pointer changing.
    rebuildDynamicSlots(ch);

    // Fast path: the per-element materials fingerprint matches what we
    // last fingerprinted, and this channel's texture array + layer map
    // are still valid. Only need to re-patch textureRefs on fs.materials
    // so the SSBO upload below carries the cached layer indices (dynamic
    // slots patched from the freshly rebuilt dynamicSlotMap).
    if(sameMaterialsContent && channel.primaryArray())
    {
      patchMaterialRefsFromCache(ch, fs);
      return false;
    }

    // Multi-bucket texture arrays. Each distinct
    // (RGBA8, imageSize) tuple goes into its own bucket. Materials
    // reference `tex_ref_static(bucket_id, layer_id)`; patchMaterial-
    // RefsFromCache walks buckets[] to emit the correct refs.
    //
    // Algorithm:
    //   1. Clear all buckets' layerMaps (we'll rebuild them).
    //   2. Walk materials, decode each unique source up-front, route
    //      it to `findOrCreateBucket(RGBA8, image.size())`. Layer
    //      indices are bucket-local.
    //   3. For each bucket that changed size/layer-count: reallocate
    //      its QRhiTextureArray at the right native size.
    //   4. Upload decoded images into their assigned (bucket, layer)
    //      slots — no scaling, sizes already match by construction.
    //   5. Ensure bucket 0 always has at least 1 fallback layer so
    //      the default `baseColorArray` binding stays valid for
    //      single-bucket-era shaders.
    //
    // Format axis reserved for future: today every bucket is RGBA8.
    // HDR emissive / wide-gamut / compressed formats plug into this
    // same mechanism by varying the format argument.

    for(auto& b : channel.buckets)
      b.layerMap.clear();

    // Decoded pending uploads + their target (bucket, layer).
    struct PendingLayer
    {
      int bucket_idx;
      int layer_idx;
      QImage image;
    };
    std::vector<PendingLayer> pendingUploads;
    pendingUploads.reserve(16);

    if(matsPtr)
    {
      // Process a single static texture_ref into this channel's bucket
      // pool. Used uniformly for both the main channel ref and every
      // ext-table ref whose `channel` matches `ch` — shared logic
      // means new ext slots automatically pick up dedup, decode-fail
      // handling, and bucket-cap diagnostics for free.
      //
      // `is_main_occlusion` enables the glTF MR-r packed-occlusion
      // shortcut, which only applies to the main occlusion channel ref
      // (an ext texture happening to share a source with MR doesn't
      // get short-circuited — semantically distinct field). When the
      // shortcut fires we also need the material's MR source pointer
      // for the comparison; passed in as `mr_source_for_occ_check`.
      const auto register_static_ref
          = [&](const ossia::texture_ref& tref,
                const ossia::texture_source* mr_source_for_occ_check,
                bool is_main_occlusion) {
        const auto* s = tref.source.get();
        if(!s)
          return;

        // Occlusion-from-MR shortcut: when the material's occlusion
        // texture and metallic-roughness texture share a source, the
        // shader will read occlusion from MR.r * factor (the canonical
        // glTF packing convention) and we don't need to allocate a
        // separate occlusion layer for this material. patchMaterial-
        // RefsFromCache also short-circuits → tex_ref_none() for the
        // occlusion ref, the shader feature_mask bit stays clear, and
        // the MR.r path takes over.
        if(is_main_occlusion && s == mr_source_for_occ_check)
          return;

        // Skip if already mapped in any bucket this walk (same source
        // referenced by N materials, or by main + ext slots on the
        // same material — single upload shared by all).
        for(const auto& b : channel.buckets)
          if(b.layerMap.find(s) != b.layerMap.end())
            return;

        // Decode now so we know the native size to pick a bucket.
        // AssetTable `peek` may return a cached QImage → zero-cost.
        QImage img = decodeTextureSource(*s, renderer.assetTable());
        if(img.isNull())
          return;

        // Heuristic: the decode-failure fallback is a 1×1 image; real
        // textures are ≥ 8 px on both axes. Skip bucket assignment on
        // clearly-degenerate results so we don't spawn a 1×1 bucket.
        if(img.width() < 8 || img.height() < 8)
          return;

        // Route to bucket keyed on (format, size, sampler_config). The
        // sampler_config split lets per-glTF-texture wrap/filter modes
        // be honoured even when several materials share a channel
        // array — distinct samplers → distinct buckets, each with its
        // own QRhiSampler. For the common case (Sponza, DamagedHelmet,
        // most glTFs use a single sampler) this collapses to one
        // bucket per (format, size).
        auto [b_idx, b_ptr] = channel.findOrCreateBucket(
            QRhiTexture::RGBA8, img.size(), tref.sampler);
        if(b_idx < 0)
        {
          qWarning().noquote()
              << "ScenePreprocessor: channel" << channelName(ch)
              << "hit bucket cap ("
              << GpuResourceRegistry::kMaxBuckets
              << "); texture_source skipped — shader will see tex_ref_none.";
          return;
        }

        const int layer = (int)b_ptr->layerMap.size();
        b_ptr->layerMap[s] = layer;
        pendingUploads.push_back({b_idx, layer, std::move(img)});
      };

      const auto register_material_refs
          = [&](const ossia::material_component& m) {
        const auto* mr_source = m.metallic_roughness_texture.source.get();
        // Main channel ref.
        if(const auto* tref = channelRef(ch, m); tref)
          register_static_ref(*tref, mr_source, ch == ChannelOcclusion);
        // Ext-table refs whose pool matches this channel.
        for(const auto& slot : kExtTextureSlots)
          if(slot.channel == ch)
            register_static_ref(slot.accessor(m), mr_source, false);
      };
      for(const auto& m : *matsPtr)
        if(m)
          register_material_refs(*m);
      // Instancer-prototype materials live outside scene_state.materials
      // (owned by the prototype mesh_component). Walk them here so their
      // textures land in the channel buckets and arenaSlotForMaterial
      // can patch resolved refs in the upload pass.
      for(const auto& inst_draw : fs.instances)
      {
        const auto* inst = inst_draw.instance.get();
        if(!inst || !inst->prototype)
          continue;
        for(const auto& prim : inst->prototype->primitives)
          if(const auto* mat = prim.material.get(); mat)
            register_material_refs(*mat);
      }
    }

    // Ensure bucket 0 exists for init-time / shader-binding stability.
    // If no material landed in it, ensurePrimary() with default size
    // gives a safe fallback target.
    if(channel.buckets.empty())
    {
      channel.ensurePrimary(
          QRhiTexture::RGBA8,
          QSize(kChannelLayerSize, kChannelLayerSize));
    }

    // Per-bucket allocate / reallocate.
    bool anyReallocated = false;
    for(std::size_t bi = 0; bi < channel.buckets.size(); ++bi)
    {
      auto& b = channel.buckets[bi];
      // At least 1 layer — empty bucket gets a fallback at layer 0.
      const int wantLayers = std::max(1, (int)b.layerMap.size());
      if(!b.array || b.layers != wantLayers)
      {
        if(b.array)
          b.array->deleteLater();
        b.array = rhi.newTextureArray(
            b.format, wantLayers, b.pixelSize, 1, channelFlags(ch));
        if(b.array)
        {
          b.array->setName(
              QByteArray("ScenePreprocessor::") + channelName(ch)
              + '[' + QByteArray::number((int)bi) + ']');
          if(!b.array->create())
          {
            delete b.array;
            b.array = nullptr;
          }
          else
          {
            b.layers = wantLayers;
            anyReallocated = true;
          }
        }
      }

      // Per-bucket QRhiSampler. Created on first allocation, kept
      // alive across rebuilds (the sampler_config is immutable for a
      // bucket — bucket identity includes it). Never recreated unless
      // the bucket is destroyed.
      if(b.array && !b.sampler)
      {
        auto wrap_to_qrhi = [](ossia::texture_address_mode m) {
          switch(m)
          {
            case ossia::REPEAT:        return QRhiSampler::Repeat;
            case ossia::CLAMP_TO_EDGE: return QRhiSampler::ClampToEdge;
            case ossia::MIRROR:        return QRhiSampler::Mirror;
          }
          return QRhiSampler::Repeat;
        };
        auto filter_to_qrhi = [](ossia::texture_filter f,
                                 QRhiSampler::Filter dflt) {
          switch(f)
          {
            case ossia::NONE:    return QRhiSampler::None;
            case ossia::NEAREST: return QRhiSampler::Nearest;
            case ossia::LINEAR:  return QRhiSampler::Linear;
          }
          return dflt;
        };
        // Material textures are always uploaded with a full mip chain
        // (TextureLoader.cpp::uploadImageToTexture: MipMapped +
        // generateMips on first upload). Force the bucket sampler to
        // trilinear-filter that chain:
        //   - mag/min filter promoted to LINEAR when the loader said
        //     NONE (NEAREST is preserved — that's an explicit author
        //     choice, e.g. pixel-art assets).
        //   - mipmap_mode promoted to LINEAR when the loader said NONE
        //     (the common case where a glTF declared minFilter=LINEAR
        //     instead of LINEAR_MIPMAP_LINEAR — without this override
        //     the GPU only ever samples mip 0 and we get the same
        //     minification noise the mipmap fix was meant to solve).
        auto promote_to_linear
            = [](ossia::texture_filter f) -> ossia::texture_filter {
          return f == ossia::NONE ? ossia::LINEAR : f;
        };
        b.sampler = rhi.newSampler(
            filter_to_qrhi(promote_to_linear(b.sampler_config.mag_filter), QRhiSampler::Linear),
            filter_to_qrhi(promote_to_linear(b.sampler_config.min_filter), QRhiSampler::Linear),
            filter_to_qrhi(promote_to_linear(b.sampler_config.mipmap_mode), QRhiSampler::Linear),
            wrap_to_qrhi(b.sampler_config.wrap_s),
            wrap_to_qrhi(b.sampler_config.wrap_t));
        b.sampler->setName(
            QByteArray("ScenePreprocessor::") + channelName(ch) + "_sampler["
            + QByteArray::number((int)bi) + ']');
        if(!b.sampler->create())
        {
          delete b.sampler;
          b.sampler = nullptr;
        }
        else
        {
          // Sampler swap forces SRB rebind on the consumer side.
          anyReallocated = true;
        }
      }
    }

    // Upload real textures into their bucket/layer slots.
    for(auto& pu : pendingUploads)
    {
      auto& b = channel.buckets[pu.bucket_idx];
      if(!b.array)
        continue;
      QImage img = std::move(pu.image);
      if(img.format() != QImage::Format_RGBA8888)
        img.convertTo(QImage::Format_RGBA8888);
      // Sizes match by construction — no scale needed.
      QRhiTextureSubresourceUploadDescription sub(img);
      QRhiTextureUploadEntry entry(pu.layer_idx, 0, sub);
      res.uploadTexture(
          b.array, QRhiTextureUploadDescription({entry}));
    }

    // Fallback for empty buckets (no real uploads): drop a neutral
    // 1-layer default so the shader's bucket-switch case for this
    // bucket doesn't sample undefined memory.
    for(std::size_t bi = 0; bi < channel.buckets.size(); ++bi)
    {
      auto& b = channel.buckets[bi];
      if(!b.array || !b.layerMap.empty())
        continue;
      QImage fallback(b.pixelSize, QImage::Format_RGBA8888);
      switch(ch)
      {
        case ChannelBaseColor:  fallback.fill(Qt::white); break;
        case ChannelEmissive:   fallback.fill(Qt::black); break;
        // MR / packed-extension fallback: white (1,1,1,1) so per-material
        // metallic_factor / roughness_factor / clearcoat_factor / sheen / etc.
        // apply via multiplication. A non-white fallback would zero out the
        // authored factors (e.g., metallic_factor=1 + no MR texture → black
        // metal instead of mirror).
        case ChannelMetalRough: fallback.fill(Qt::white); break;
        case ChannelNormal:     fallback.fill(QColor(128, 128, 255, 255)); break;
        default:                fallback.fill(Qt::white); break;
      }
      QRhiTextureSubresourceUploadDescription sub(fallback);
      QRhiTextureUploadEntry entry(0, 0, sub);
      res.uploadTexture(
          b.array, QRhiTextureUploadDescription({entry}));
    }

    // `arrayReallocated` is the rebuildChannel return value: when any
    // bucket's QRhiTexture* was recreated, downstream SRBs need a
    // rebind. Caller threads it through the "auxBuffersChanged"
    // flag in update().
    const bool arrayReallocated = anyReallocated;

    // Per-channel diagnostic — tells you bucket count, per-bucket size,
    // layer count, and how many sources got dropped. Critical for
    // understanding "missing textures" symptoms (e.g. Sponza mat 2
    // dropped because white.png is 4×4, below the <8 px decode floor).
    if(buftrace_enabled())
    {
      QString detail;
      detail.reserve(128);
      for(std::size_t bi = 0; bi < channel.buckets.size(); ++bi)
      {
        const auto& b = channel.buckets[bi];
        detail += QStringLiteral(" b%1=%2x%3×%4")
                      .arg(bi)
                      .arg(b.pixelSize.width())
                      .arg(b.pixelSize.height())
                      .arg(b.layers);
      }
      BUFTRACE() << "[Channel " << channelName(ch)
                 << "] buckets=" << channel.buckets.size()
                 << " pendingUploads=" << pendingUploads.size()
                 << detail
                 << " realloc=" << anyReallocated;
    }

    patchMaterialRefsFromCache(ch, fs);
    return arrayReallocated;
  }

  // Walk fs.materials in lockstep with scene.state->materials and set
  // textureRefs[ch] from channel's layerMap. Called from both the fast
  // path (same materials list) and the rebuild path (materials list
  // changed).
  void patchMaterialRefsFromCache(MaterialChannel ch, FlatScene& fs)
  {
    if(!this->scene.state || !this->scene.state->materials || !m_registry)
      return;
    const auto& mats = *this->scene.state->materials;
    const auto& channel = texChannel(ch);
    const auto& dynMap = channel.dynamicSlotMap;
    const std::size_t n = std::min(fs.materials.size(), mats.size());
    const std::size_t n_ext = std::min(n, fs.material_extensions.size());

    // Channel 4 (Occlusion) lives in `MaterialGPU::occlusion_textureRef`,
    // a single uint32 outside the 4-element textureRefs uvec4 (which
    // holds BC/MR/Normal/Em only). Branch out the storage target so we
    // don't write OOB into textureRefs[4].
    const auto write_main_ref
        = [ch](MaterialGPU& m, uint32_t ref) noexcept {
      if(ch == ChannelOcclusion)
        m.occlusion_textureRef = ref;
      else
        m.textureRefs[ch] = ref;
    };

    // Encode a single texture_ref into a packed uint per the
    // tex_ref_static / tex_ref_dynamic / tex_ref_none scheme. Looks up
    // the dynamic handle in this channel's slotMap first (since GPU
    // handles take precedence over CPU sources when both are set —
    // mirrors the rebuild walker's order). Static sources are matched
    // against the per-bucket layerMap that rebuildChannel populated.
    // Returns tex_ref_none() for empty refs OR refs that overflowed
    // the dynamic slot cap OR static sources we failed to map (decode
    // failure, bucket cap, etc.).
    const auto encode_ref = [&](const ossia::texture_ref& tref) -> uint32_t {
      // Dynamic path: GPU handle without a CPU source.
      if(!tref.source && tref.texture.valid())
      {
        // Look up by globalResourceId — see GpuResourceRegistry.cpp's
        // resolveDynamicSlot for the recycling-safety rationale.
        auto* dynTex
            = static_cast<QRhiTexture*>(tref.texture.native_handle);
        auto it
            = dynTex ? dynMap.find(dynTex->globalResourceId()) : dynMap.end();
        return (it != dynMap.end())
                   ? tex_ref_dynamic((uint32_t)it->second)
                   : tex_ref_none();
      }
      // Static path: walk this channel's buckets for the source pointer.
      if(const auto* s = tref.source.get(); s)
      {
        for(std::size_t bi = 0; bi < channel.buckets.size(); ++bi)
        {
          auto it = channel.buckets[bi].layerMap.find(s);
          if(it != channel.buckets[bi].layerMap.end())
            return tex_ref_static((uint32_t)bi, (uint32_t)it->second);
        }
      }
      return tex_ref_none();
    };

    for(std::size_t i = 0; i < n; ++i)
    {
      // Null-material clear: zero out main + all ext slots mapped to
      // this channel so a transient nullptr in mats[i] doesn't leave
      // stale refs from the previous frame.
      if(!mats[i])
      {
        write_main_ref(fs.materials[i], tex_ref_none());
        if(i < n_ext)
          for(const auto& slot : kExtTextureSlots)
            if(slot.channel == ch)
              fs.material_extensions[i].textureRefs[slot.slot]
                  = tex_ref_none();
        continue;
      }

      // ── Main channel ref ──────────────────────────────────────────
      // Occlusion-from-MR shortcut (see rebuildChannel above): when
      // the source is shared with MR, leave the ref as none so the
      // shader takes the MR.r packed-occlusion path.
      const auto* main_tref = channelRef(ch, *mats[i]);
      const bool occ_packed_in_mr
          = (ch == ChannelOcclusion
             && main_tref
             && main_tref->source
             && main_tref->source.get()
                    == mats[i]->metallic_roughness_texture.source.get());
      write_main_ref(
          fs.materials[i],
          (main_tref && !occ_packed_in_mr)
              ? encode_ref(*main_tref)
              : tex_ref_none());

      // ── Ext-slot refs ─────────────────────────────────────────────
      // For each ext slot whose pool is `ch`, encode and write to
      // MaterialExtensionsGPU::textureRefs[slot]. Slots whose pool
      // ≠ ch are written by other rebuildChannel(ch') passes — over
      // ChannelCount calls per frame, every slot mapped in
      // kExtTextureSlots gets its turn.
      if(i < n_ext)
      {
        for(const auto& slot : kExtTextureSlots)
        {
          if(slot.channel != ch)
            continue;
          fs.material_extensions[i].textureRefs[slot.slot]
              = encode_ref(slot.accessor(*mats[i]));
        }
      }
    }
  }

  // Append all non-null material-texture channels + skybox to the emitted
  // geometry as auxiliary_texture entries. Consumer shaders auto-resolve
  // by name (base_color_array / metal_rough_array / normal_array /
  // emissive_array / skybox) via try_bind_texture_from_geometry — no
  // manual cable required. Null handles are filtered out so a shader
  // missing a given channel falls back to its own sampler default.
  void appendTextureAuxes(ossia::geometry& g) const
  {
    if(!m_registry)
      return;
    for(int i = 0; i < ChannelCount; ++i)
    {
      auto ch = static_cast<MaterialChannel>(i);
      const auto& channel = texChannel(ch);

      // Emit one `auxiliary_texture` per live bucket,
      // named `<channelName><bucket_id>` (e.g. `baseColorArray0`,
      // `baseColorArray1`, …). Consumer shaders declare matching
      // sampler2DArray INPUTS per bucket and switch on the 6-bit
      // `bucket` field from MaterialGPU::textureRefs. Capped at
      // kMaxBuckets.
      //
      // Back-compat alias: bucket 0 is ALSO emitted under the
      // unsuffixed name `<channelName>` (e.g. `baseColorArray`). That
      // keeps single-bucket-era shaders (classic_pbr, classic_pbr_textured,
      // etc.) rendering correctly — they only decode bucket 0's
      // layers and ignore the higher bits. Multi-bucket scenes that
      // hit a non-zero bucket through one of those shaders will
      // render bucket 0's layer in place of the intended bucket
      // (visibly wrong); users hitting that path should migrate to
      // classic_pbr_full or a ladder-aware preset. Zero overhead for
      // single-bucket scenes, which remain the common case.
      for(std::size_t bi = 0; bi < channel.buckets.size(); ++bi)
      {
        auto* tex = channel.buckets[bi].array;
        if(!tex)
          continue;
        // sampler_handle is null when the bucket is the init-time
        // fallback (bucket 0 with no real sources). Renderer falls
        // back to its own shader-config sampler when null. Real
        // material buckets populate the per-bucket sampler in
        // rebuildChannel above so per-glTF-texture wrap/filter
        // modes propagate end-to-end.
        void* sampler_h = static_cast<void*>(channel.buckets[bi].sampler);
        // Suffixed, always.
        g.auxiliary_textures.push_back(
            {.name = std::string(channelName(ch))
                     + std::to_string((int)bi),
             .native_handle = tex,
             .sampler_handle = sampler_h});
        // Unsuffixed alias only for bucket 0.
        if(bi == 0)
        {
          g.auxiliary_textures.push_back(
              {.name = channelName(ch),
               .native_handle = tex,
               .sampler_handle = sampler_h});
        }
      }
      // Dynamic slot textures: one aux entry per used slot, named
      // `<channelDynBase><slot>` (e.g., "baseColorDyn0"). Consumer
      // shaders declare matching sampler2D uniforms and branch on the
      // textureRefs source bits to pick static array vs dyn sampler.
      const auto& dyn = texChannel(ch).dynamicTextures;
      const char* dynBase = channelDynBaseName(ch);
      for(int s = 0; s < (int)dyn.size(); ++s)
      {
        if(auto* tex = dyn[s])
        {
          g.auxiliary_textures.push_back(
              {.name = std::string(dynBase) + std::to_string(s),
               .native_handle = tex});
        }
      }
    }
    if(this->scene.state)
    {
      // Scene-wide environment textures, exposed under well-known aux
      // names. Consumer shaders declare matching INPUTS (e.g.
      // `{"NAME": "irradiance_map", "TYPE": "cubemap"}`) and the
      // existing aux-resolver picks them up over the already-wired
      // scene cable. No hidden dataflow: the scene cable is explicit;
      // we're just publishing named sub-resources onto it (same
      // pattern as skybox, base_color_array, etc.).
      const auto& env = this->scene.state->environment;
      if(auto* skybox = static_cast<QRhiTexture*>(
             env.skybox_texture.native_handle))
      {
        g.auxiliary_textures.push_back(
            {.name = "skybox", .native_handle = skybox});
      }
      if(auto* t = static_cast<QRhiTexture*>(env.irradiance_map.native_handle))
      {
        g.auxiliary_textures.push_back(
            {.name = "irradiance_map", .native_handle = t});
      }
      if(auto* t = static_cast<QRhiTexture*>(env.prefiltered_map.native_handle))
      {
        g.auxiliary_textures.push_back(
            {.name = "prefiltered_map", .native_handle = t});
      }
      if(auto* t = static_cast<QRhiTexture*>(env.brdf_lut.native_handle))
      {
        g.auxiliary_textures.push_back(
            {.name = "brdf_lut", .native_handle = t});
      }
      // Shadow-map array lives off scene_state (not environment) since
      // it's tied to the shadow_cascades_info authored by
      // ShadowCascadeSetup.
      if(auto* t = static_cast<QRhiTexture*>(
             this->scene.state->shadow_cascades.shadow_map_array
                 .native_handle))
      {
        g.auxiliary_textures.push_back(
            {.name = "shadow_map_array", .native_handle = t});
      }
    }
  }

  // Texture outputs have been removed — every material-texture array and
  // the skybox now ride along on the Geometry output as auxiliary_texture
  // entries. Left in place only to satisfy the virtual override; the
  // single remaining output port (Geometry) never takes this path.
  QRhiTexture* textureForOutput(const Port& /*output*/) override
  {
    return nullptr;
  }

  // Pack every camera collected by flattenScene into a std140 UBO array.
  // Slot 0 is always the active camera; remaining slots are the other
  // cameras in insertion order. If the scene has no cameras we synthesize a
  // single default entry so downstream shaders always have a valid binding.
  //
  // Diff-uploads against m_cachedCameras to avoid Dynamic-buffer churn when
  // camera parameters don't change frame to frame.
  void packAndUploadCameras(
      RenderList& renderer, QRhiResourceUpdateBatch& res, const FlatScene& fs)
  {
    // Per-frame idempotency. update() is dispatched once per outgoing
    // edge — running this function more than once in the same frame
    // would corrupt camera_prev: the snapshot-before-overwrite step
    // (line below) reads m_cachedCameras to seed camera_prev, then
    // overwrites m_cachedCameras with the new fresh. A second call
    // within the same frame would snapshot the just-overwritten
    // (current-frame) data into camera_prev → camera_prev == camera →
    // motion = 0 even on real motion frames. RenderList::frame is
    // incremented at the end of each renderInternal pass, so it's a
    // reliable per-frame token here.
    if(m_lastCameraUploadFrame == renderer.frame)
      return;

    auto& rhi = *renderer.state.rhi;
    // Prefer the scene's explicit render target size when an upstream
    // producer (EnvironmentLoader / SetRenderTarget-style node) has
    // stamped one — that size is correct for whatever off-screen pass
    // this preprocessor drives. Fall back to the RenderList's swap-chain
    // size, which is only right for the main window pass.
    QSize rsize = renderer.state.renderSize;
    if(this->scene.state)
    {
      const auto& env = this->scene.state->environment;
      if((env.params_set & ossia::scene_environment::params_render_target_size)
         && env.render_target_size[0] > 0
         && env.render_target_size[1] > 0)
      {
        rsize = QSize(
            (int)env.render_target_size[0],
            (int)env.render_target_size[1]);
      }
    }

    std::vector<CameraUBOData> fresh;
    if(fs.cameras.empty())
    {
      // Default camera used when no camera is present in the scene.
      ossia::camera_component cam{};
      QMatrix4x4 view;
      view.lookAt(
          QVector3D(0.f, 1.f, 3.f), QVector3D(0.f, 0.f, 0.f),
          QVector3D(0.f, 1.f, 0.f));
      CameraUBOData d{};
      packCameraUBO(d, cam, view.inverted(), rsize, 0.f);
      fresh.push_back(d);
    }
    else
    {
      fresh.reserve(fs.cameras.size());
      // Put the active camera first so shaders that index by 0 pick it up
      // without knowing about activeCameraIndex.
      const int active = std::max(0, fs.activeCameraIndex);
      auto packOne = [&](const FlatScene::CameraEntry& e) {
        CameraUBOData d{};
        packCameraUBO(d, *e.component, e.worldTransform, rsize, 0.f);
        fresh.push_back(d);
      };
      packOne(fs.cameras[(std::size_t)active]);
      for(std::size_t i = 0; i < fs.cameras.size(); ++i)
      {
        if((int)i != active)
          packOne(fs.cameras[i]);
      }
    }

    const int64_t bytes = (int64_t)(fresh.size() * sizeof(CameraUBOData));

    // Pre-allocate a large enough capacity so the buffer pointer is stable
    // across typical scene changes — aux-buffer bindings downstream resolve
    // to this QRhiBuffer* at geometry-rebuild time, and growing invalidates
    // those bindings. 16 cameras × 240 B = 3840 B covers every realistic
    // multi-view case (cubemap = 6, stereo = 2, typical single = 1).
    constexpr int64_t kMinCap = 16 * (int64_t)sizeof(CameraUBOData);
    const int64_t wantCap = std::max(bytes, kMinCap);

    if(!m_camerasBuffer || m_camerasCap < wantCap)
    {
      if(m_camerasBuffer)
        renderer.releaseBuffer(m_camerasBuffer);
      if(m_camerasPrevBuffer)
        renderer.releaseBuffer(m_camerasPrevBuffer);
      m_camerasBuffer = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, (quint32)wantCap);
      m_camerasBuffer->setName("ScenePreprocessor::cameras");
      m_camerasBuffer->create();
      m_camerasPrevBuffer = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, (quint32)wantCap);
      m_camerasPrevBuffer->setName("ScenePreprocessor::cameras_prev");
      m_camerasPrevBuffer->create();
      m_camerasCap = wantCap;
      m_cachedCameras.clear();
      // Force the upload below to actually run after realloc — the
      // freshly created buffers contain garbage and must be filled.
      m_lastCameraUploadFrame = -1;
    }

    // Upload `camera_prev` from the CPU mirror of what's currently in the
    // GPU `camera` buffer (= last frame's content, since we're about to
    // overwrite it with `fresh` below). On the first frame m_cachedCameras
    // is empty — seed prev with current so MV = 0 (no history snap).
    //
    // Earlier impl held a separate m_prevCameras shadow that was only
    // refreshed on cache MISS, while the prev-buffer upload ran every
    // frame. With cache-hit/miss alternation (renderSize toggles, multi-
    // producer env-merge order, animation tick != render tick) this left
    // camera_prev lagging by 2 frames on the post-hit miss frame —
    // GPU camera_prev ended up byte-equal to GPU camera, so motion = 0
    // every other frame and downstream temporal upscalers / reproject
    // shaders flickered between correct and zero output.
    //
    // Mirroring m_worldTransformsPrevBuffer's pattern (snapshot-current-
    // before-overwrite) makes the prev semantic a function of the GPU
    // buffer's last frame content, not of cache-hit history. Always
    // upload current too — the diff-skip saved <4 KB of Dynamic-UBO
    // churn per frame and was the source of the bug.
    const auto& prevPayload
        = m_cachedCameras.empty() ? fresh : m_cachedCameras;
    const int64_t prevBytes
        = (int64_t)(prevPayload.size() * sizeof(CameraUBOData));
    res.updateDynamicBuffer(
        m_camerasPrevBuffer, 0, (quint32)prevBytes, prevPayload.data());

    res.updateDynamicBuffer(m_camerasBuffer, 0, (quint32)bytes, fresh.data());
    m_cachedCameras = std::move(fresh);
    m_lastCameraUploadFrame = renderer.frame;

    // The camera UBO isn't exposed on an external output port anymore —
    // it rides along on the geometry as the `camera` auxiliary buffer
    // (attached in rebuildMDI), so try_bind_from_geometry resolves the
    // shader's `uniform camera` input by name without a dedicated cable.
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge*) override
  {
    // Re-flatten when the CONTENT actually changed, not just when a push
    // occurred this frame. Producers (glTF/FBX loaders, Light)
    // now re-push every frame so that multi-source scenes stay consistent
    // across frames; the merge cache in NodeRenderer keeps the resulting
    // scene_state shared_ptr stable when no input changed. That makes the
    // pointer + version check a reliable "did the content change" test,
    // and we can skip the sceneChanged forced-rebuild entirely.
    bool needsRebuild = !m_outputSpec.meshes;
    if(this->scene.state.get() != m_cachedSceneState)
      needsRebuild = true;
    if(this->scene.state && this->scene.state->version != m_cachedVersion)
      needsRebuild = true;

    // Always refresh the camera UBOs every frame, regardless of whether
    // mesh-rebuild fires. Decoupling camera updates from the rebuild gate
    // is required for motion-vector reprojection to be correct:
    //
    //   * "Camera moves, then stops": without per-frame upload, the last
    //     rebuild leaves camera_prev = old, camera = new in the GPU UBOs.
    //     scene_state stops bumping its version → no further rebuild →
    //     UBOs frozen at the motion-in-progress state → motion-vector
    //     consumers see ghost motion forever after the camera stopped.
    //
    //   * "Static camera + animated geometry": some scene producers bump
    //     scene_state.version on transform changes, others don't. If the
    //     gate misses, the camera UBO never updates even when the camera
    //     does change. Always running packAndUploadCameras here makes
    //     motion-vector correctness independent of which producer is in
    //     play.
    //
    // packAndUploadCameras synthesises a default camera when fs.cameras
    // is empty, so this runs unconditionally — keeps m_camerasBuffer
    // allocated and bound even when no scene producer is wired yet.
    //
    // Per-frame guard: update() is dispatched once per
    // outgoing edge, and packAndUploadCameras already early-returns when
    // it has already run this frame (m_lastCameraUploadFrame ==
    // renderer.frame). But the flattenScene() feeding it is NOT free — it
    // packs every material, runs skeleton FK and allocates a shared_ptr
    // wrapper per primitive — so running it once per edge wastes that work
    // on edges 2..K whose packAndUploadCameras is a no-op anyway. Gate the
    // whole camera flatten+upload on the same per-frame token so it runs at
    // most once per frame regardless of edge count.
    if(m_lastCameraUploadFrame != renderer.frame)
    {
      FlatScene cameraFs;
      flattenScene(this->scene, cameraFs, /*aspectRatio=*/1.f);
      packAndUploadCameras(renderer, res, cameraFs);
    }

    if(!needsRebuild)
    {
      // Still consume the sceneChanged flag so we don't loop on it forever.
      this->sceneChanged = false;
      return;
    }

    BUFTRACE() << "ScenePreprocessor::update REBUILD cached_state="
               << (const void*)m_cachedSceneState
               << " cached_ver=" << (qint64)m_cachedVersion
               << " new_state=" << (void*)this->scene.state.get()
               << " new_ver="
               << (this->scene.state ? (qint64)this->scene.state->version : (qint64)-1)
               << " mdi_indices="
               << (void*)(m_registry ? m_registry->meshStreamBuffer(
                       GpuResourceRegistry::MeshStream::Indices) : nullptr)
               << " (downstream shader bindings still reference the "
                  "pre-rebuild MDI buffers until the next acquireMesh)";

    // Walk the scene. flattenScene is O(nodes) — cheap compared to any
    // GPU upload — so we always do it. The expensive work (vertex/index
    // concat + upload) is then gated by the mesh fingerprint below.
    {
      FlatScene fs;
      flattenScene(this->scene, fs, /*aspectRatio=*/1.f);

      std::vector<uint32_t> materialTagHashes;
      if(this->scene.state && this->scene.state->materials)
      {
        const auto& mats = *this->scene.state->materials;
        materialTagHashes.reserve(mats.size());
        for(const auto& m : mats)
          materialTagHashes.push_back(
              m ? (uint32_t)ossia::hash_string(m->tag) : 0u);
      }

      // Allocate Material arena slots for every loader material (materials
      // entering the scene without a live producer's raw_slot) + upload
      // MaterialGPU bytes. Producer-authored materials already have valid
      // slots kept fresh by their own update(); we skip those here.
      // Slot allocation persists across frames via m_loaderMaterialSlots —
      // cheap cache hit for scenes that don't change. When a material
      // disappears (removed from scene_state.materials), its slot is
      // reclaimed by the garbage-collection pass below.
      if(this->scene.state && m_registry)
      {
        const std::vector<ossia::material_component_ptr> empty_mats;
        const auto& mats = this->scene.state->materials
                               ? *this->scene.state->materials
                               : empty_mats;
        ossia::hash_set<const ossia::material_component*> seen;
        seen.reserve(mats.size() + fs.instances.size());
        const auto register_loader_material
            = [&](const ossia::material_component* mat) {
          if(!mat)
            return;
          seen.insert(mat);
          // Producer-authored material: its own update() maintains the
          // slot contents every frame. Skip.
          if(m_registry->isLive(mat->raw_slot))
            return;
          // Loader material: allocate a slot on first sight, upload
          // packed MaterialGPU bytes. No per-frame re-upload: loader
          // materials are immutable between file-loads, so the slot
          // bytes we wrote on first sight are still valid.
          auto [it, inserted]
              = m_loaderMaterialSlots.emplace(mat, GpuResourceRegistry::Slot{});
          if(inserted)
          {
            it->second = m_registry->allocate(
                GpuResourceRegistry::Arena::Material, sizeof(MaterialGPU));
            // No upload here — textureRefs aren't resolved yet. The
            // upload happens after the rebuildChannel loop, once the
            // per-channel layerMaps know which source lands on which
            // layer. Arena-full case: the GC pass below drops the
            // invalid entry on the next material list change.
          }
        };
        for(const auto& mat_ptr : mats)
          register_loader_material(mat_ptr.get());
        // Instancer prototypes carry their own material_component
        // pointers that aren't in scene_state.materials (they're owned
        // by the prototype mesh_component). Without registering them
        // here, arenaSlotForMaterial(prim.material) falls back to slot
        // 0 (the seedDefaults white-dielectric) and every loader-built
        // instance group renders with that default.
        for(const auto& inst_draw : fs.instances)
        {
          const auto* inst = inst_draw.instance.get();
          if(!inst || !inst->prototype)
            continue;
          for(const auto& prim : inst->prototype->primitives)
            register_loader_material(prim.material.get());
        }
        // Garbage-collect slots whose materials disappeared from the
        // scene. Scanning after the allocation pass ensures entries
        // still present are kept.
        for(auto it = m_loaderMaterialSlots.begin();
            it != m_loaderMaterialSlots.end();)
        {
          if(seen.find(it->first) == seen.end())
          {
            if(it->second.valid())
              m_registry->free(it->second);
            it = m_loaderMaterialSlots.erase(it);
          }
          else
          {
            ++it;
          }
        }
      }

      // Build / refresh every material-texture channel AND patch
      // fs.materials[i].textureRefs[ch] with the assigned layer indices.
      // Must happen before the scene_materials SSBO upload below so
      // materials are written with the right refs.
      //
      // Each channel has its own QRhiTextureArray (sRGB for base color
      // & emissive, linear for MR & normal — see channelFlags). When a
      // channel's QRhiTexture* gets reallocated (layer count grew, …)
      // the emitted auxiliary_texture entry's native_handle changes —
      // downstream's rebindAuxTextures picks that up via the per-frame
      // geometry lookup, but ONLY if downstream's geometryChanged fires,
      // which requires a fresh meshes shared_ptr. Roll the realloc
      // signal into the same `auxBuffersChanged` flag the SSBO-grow path
      // uses: rebuildMDI() rebuilds the meshes vector every time that
      // flag fires, giving the downstream a pointer identity change.
      //
      // Fingerprint the materials list once and pass the equality result
      // to each channel so we don't re-walk the list ChannelCount times.
      std::vector<uint64_t> fingerprint;
      computeMaterialsFingerprint(fingerprint);
      // Append prototype-material identity into the fingerprint so a
      // prototype-only change (model swap, variant select) re-triggers
      // the channel rebuild + upload below.
      for(const auto& inst_draw : fs.instances)
      {
        const auto* inst = inst_draw.instance.get();
        if(!inst || !inst->prototype)
          continue;
        for(const auto& prim : inst->prototype->primitives)
        {
          const auto* mat = prim.material.get();
          fingerprint.push_back(
              mat
                  ? (mat->stable_id != 0
                         ? mat->stable_id
                         : reinterpret_cast<uint64_t>(mat))
                  : 0u);
        }
      }
      const bool sameMaterialsContent
          = (fingerprint == m_cachedMaterialsFingerprint);

      bool channelReallocated = false;
      for(int i = 0; i < ChannelCount; ++i)
      {
        if(rebuildChannel(
               static_cast<MaterialChannel>(i), sameMaterialsContent,
               renderer, res, fs))
          channelReallocated = true;
      }
      if(!sameMaterialsContent)
        m_cachedMaterialsFingerprint = std::move(fingerprint);

      // Loader-material arena slot upload: now that rebuildChannel has
      // patched fs.materials[i].textureRefs with the resolved per-channel
      // layer indices, stream each loader material's packed MaterialGPU
      // bytes into its Material arena slot. Producer-authored materials
      // (PBRMesh, MaterialOverride-if-migrated, CSF mesh producers) keep
      // their own slot fresh in their update() hooks — we skip those.
      //
      // Uploads happen only when the materials content actually changed
      // (sameMaterialsContent==false) OR when a channel reallocated and
      // shifted layer indices. Steady-state frames with an unchanged
      // scene touch zero bytes here.
      if(m_registry && this->scene.state
         && (!sameMaterialsContent || channelReallocated))
      {
        const std::vector<ossia::material_component_ptr> empty_mats;
        const auto& mats = this->scene.state->materials
                               ? *this->scene.state->materials
                               : empty_mats;
        const std::size_t n
            = std::min(fs.materials.size(), mats.size());
        for(std::size_t i = 0; i < n; ++i)
        {
          const auto* mat = mats[i].get();
          if(!mat)
            continue;
          if(m_registry->isLive(mat->raw_slot))
            continue;  // producer-authored — slot owned by producer
          auto it = m_loaderMaterialSlots.find(mat);
          if(it == m_loaderMaterialSlots.end() || !it->second.valid())
            continue;
          m_registry->updateSlot(
              res, it->second, &fs.materials[i], sizeof(MaterialGPU));
        }
        // Instancer-prototype materials registered above also need
        // their MaterialGPU bytes uploaded — they aren't in
        // fs.materials so we pack on the fly. textureRefs come from the
        // rebuildChannel walk (which now also visits prototype
        // materials) so dedup with channel buckets is preserved.
        ossia::hash_set<const ossia::material_component*> uploaded;
        uploaded.reserve(mats.size() + fs.instances.size());
        for(const auto& mp : mats)
          if(mp)
            uploaded.insert(mp.get());
        for(const auto& inst_draw : fs.instances)
        {
          const auto* inst = inst_draw.instance.get();
          if(!inst || !inst->prototype)
            continue;
          for(const auto& prim : inst->prototype->primitives)
          {
            const auto* mat = prim.material.get();
            if(!mat)
              continue;
            if(!uploaded.insert(mat).second)
              continue; // shared with scene material or another prim
            if(m_registry->isLive(mat->raw_slot))
              continue;
            auto it = m_loaderMaterialSlots.find(mat);
            if(it == m_loaderMaterialSlots.end() || !it->second.valid())
              continue;
            MaterialGPU packed = packMaterial(*mat);
            // Patch textureRefs from the per-channel buckets. Mirrors
            // patchMaterialRefsFromCache but inline since prototype
            // materials aren't in fs.materials.
            for(int chi = 0; chi < ChannelCount; ++chi)
            {
              const auto ch = static_cast<MaterialChannel>(chi);
              const auto& channel = texChannel(ch);
              uint32_t ref = tex_ref_none();
              if(const auto* tref = channelRef(ch, *mat); tref)
              {
                if(!tref->source && tref->texture.valid())
                {
                  // Stable-id keyed (GpuResourceRegistry.cpp).
                  auto* dynTex = static_cast<QRhiTexture*>(
                      tref->texture.native_handle);
                  auto dit = dynTex
                                 ? channel.dynamicSlotMap.find(
                                       dynTex->globalResourceId())
                                 : channel.dynamicSlotMap.end();
                  if(dit != channel.dynamicSlotMap.end())
                    ref = tex_ref_dynamic((uint32_t)dit->second);
                }
                else if(const auto* s = tref->source.get(); s)
                {
                  for(std::size_t bi = 0; bi < channel.buckets.size(); ++bi)
                  {
                    auto bit = channel.buckets[bi].layerMap.find(s);
                    if(bit != channel.buckets[bi].layerMap.end())
                    {
                      ref = tex_ref_static(
                          (uint32_t)bi, (uint32_t)bit->second);
                      break;
                    }
                  }
                }
              }
              if(ch == ChannelOcclusion)
                packed.occlusion_textureRef = ref;
              else
                packed.textureRefs[chi] = ref;
            }
            m_registry->updateSlot(
                res, it->second, &packed, sizeof(MaterialGPU));
          }
        }
      }

      // Ensure the scene-wide SSBOs exist at a large-enough capacity. Only
      // allocates / resizes when the count grew past the current cap; the
      // common steady-state case is a no-op.
      //
      // Both `scene_materials_ext` and `scene_material_uv_xforms` are
      // indexed by Material ARENA SLOT in the shader (shader does
      // `entries[pd.material_index]` where pd.material_index is the
      // arena slot, parallel to `scene_materials` which IS the arena).
      // Their CPU side must therefore be sized + filled by arena slot
      // too, NOT by fs.materials position. See the freshMaterialUVTransforms
      // build below for the same arena-slot-indexed pattern.
      uint32_t maxArenaSlot = 0;
      if(this->scene.state && this->scene.state->materials)
      {
        for(const auto& m : *this->scene.state->materials)
        {
          if(!m)
            continue;
          maxArenaSlot
              = std::max(maxArenaSlot, arenaSlotForMaterial(m.get()));
        }
      }
      // Instancer / loader prototype materials are NOT in
      // scene.state->materials but DO get an arena slot via
      // m_loaderMaterialSlots (registered above), and their slot is what
      // arenaSlotForMaterial() — hence PerDrawGPU.material_index — resolves
      // to for those draws. If such a slot exceeds the scene-material max,
      // the shader's `scene_materials_ext[material_index]` /
      // `uv_xforms[material_index]` would read past the bound aux range
      // out of bounds. Fold those slots into the extent so the aux buffers
      // are sized to cover every reachable material_index.
      for(const auto& [mat, slot] : m_loaderMaterialSlots)
      {
        if(slot.valid())
          maxArenaSlot = std::max(maxArenaSlot, slot.slot_index);
      }
      const std::size_t arenaSlotEntries
          = (std::size_t)maxArenaSlot + 1;
      const int64_t matsExtBytes
          = std::max<int64_t>(
              16,
              (int64_t)arenaSlotEntries * sizeof(MaterialExtensionsGPU));
      auto& rhi = *renderer.state.rhi;
      // Track buffer-pointer churn: when grow reallocates any aux buffer we
      // MUST republish m_outputSpec.meshes so downstream's SRB rebinds to
      // the new pointer. Otherwise the sink keeps its old aux.buffer
      // (released via RenderList::releaseBuffer) and reads undefined memory.
      // Channel-array reallocation also counts as an aux change for the
      // purposes of bumping the mesh identity downstream — see the
      // rebuildChannel call above.
      bool auxBuffersChanged = channelReallocated;
      // Returns true on (re)allocation. Same prefix-staleness invariant
      // as the static growBuf above: callers MUST clear the matching
      // diffUpload mirror on `true` so the new (uninitialised) buffer
      // gets the full fresh contents instead of just the appended tail.
      // Also zero-fills the freshly allocated buffer (Vulkan does NOT
      // zero VkBuffers on creation — sparse-uploaded SSBOs would
      // otherwise read garbage from device-memory pages).
      auto grow = [&](QRhiBuffer*& buf, int64_t& cap, int64_t need, const char* nm) {
        if(buf && cap >= need) return false;
        int64_t newCap = cap > 0 ? cap : 16;
        while(newCap < need) newCap *= 2;
        if(buf) renderer.releaseBuffer(buf);
        buf = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, newCap);
        buf->setName(nm);
        buf->create();
        // Zero-fill via the thread-local zero pool (see RhiClearBuffer.hpp).
        RhiClearBuffer::clearBuffer(rhi, res, buf, 0, (quint32)newCap);
        cap = newCap;
        auxBuffersChanged = true;
        return true;
      };
      // scene_lights now points at the RawLight arena (fixed capacity)
      // and scene_materials points at the Material arena — no grow here
      // for either.
      // Realloc → clear the diffUpload mirror (lines 4740 / 4742) so the
      // freshly-allocated GPU buffer's prefix isn't left as garbage.
      // Same prefix-staleness invariant as growBuf — see its comment.
      if(grow(m_materialsExtBuffer, m_materialsExtCap, matsExtBytes,
              "ScenePreprocessor::materials_ext"))
        m_cachedMaterialExt.clear();

      // Per-material UV transforms (KHR_texture_transform). Sized by
      // arena-slot count (see comment above scene_materials_ext); the
      // freshMaterialUVTransforms vector built below uses the same
      // indexing.
      const int64_t uvXformBytes
          = std::max<int64_t>(
              16,
              (int64_t)arenaSlotEntries * sizeof(MaterialUVTransformGPU));
      if(grow(m_materialUVTransformsBuffer, m_materialUVTransformsCap, uvXformBytes,
              "ScenePreprocessor::material_uv_xforms"))
        m_cachedMaterialUVTransforms.clear();
      // scene_light_indices: compact uint array of arena slot indices.
      // Count the lights with valid arena slots (filter out 0xFFFFFFFF
      // sentinels from producer-less lights).
      std::vector<uint32_t> freshLightIndices;
      freshLightIndices.reserve(fs.lightArenaSlots.size());
      for(uint32_t s : fs.lightArenaSlots)
        if(s != 0xFFFFFFFFu)
          freshLightIndices.push_back(s);
      // 16 KiB floor (= 4096 light index slots) so override CSFs like
      // pack_lights_from_points / wander_lights_inline / grid_lights_inline
      // can publish up to 4k procedural lights without OOB-clamping
      // themselves to the scene-graph-derived size. RawLight arena
      // (GpuResourceRegistry::Arena::RawLight, currently 4096 slots) is
      // the matching ceiling — keep the two values consistent: this
      // floor must equal arena_slot_count * 4 bytes. If you bump one
      // without the other, either (a) procedural CSFs hit the lower
      // bound and clamp early, or (b) scene_light_indices references
      // slot indices past the arena size and rasterizers read garbage.
      const int64_t lightIdxBytes
          = std::max<int64_t>(16384, (int64_t)freshLightIndices.size() * 4);
      if(grow(m_lightIndicesBuffer, m_lightIndicesCap, lightIdxBytes,
              "ScenePreprocessor::light_indices"))
        m_cachedLightIndices.clear();

      // Allocate the scene_counts buffer once (16 bytes, never grows).
      //
      // Usage: Static + StorageBuffer (SSBO-only).
      //
      // Historical context: this buffer used to be allocated as
      // UniformBuffer | StorageBuffer to satisfy a dual-bind contract —
      // rasterizers declared `scene_counts` with TYPE: "uniform" (UBO
      // bind) while override CSFs (pack_lights_from_points etc.)
      // declared the same name with ACCESS: "read_write" (SSBO bind).
      // QRhi forbids Dynamic + StorageBuffer, so the buffer had to be
      // Static. But D3D11 / GLES don't support NonDynamicUniformBuffers
      // — `Static + UniformBuffer` fails create() silently there, and
      // the override-CSF write pattern was unreachable on every desktop
      // backend except Vulkan / Metal / D3D12.
      //
      // Resolution: drop the UBO half entirely. All bundled shaders
      // (presets/rasterizers/*.frag, presets/filters/*.csf,
      // presets/lighting/*.csf, presets/volumetric/*.csf) declare
      // `scene_counts` as a storage buffer. Rasterizers (top-level
      // INPUTS) declare it with `TYPE: "storage", ACCESS: "read_only"`
      // → parser emits `layout(std430) readonly buffer scene_counts_buf
      // { ... } scene_counts;`. Filters / lighting / volumetric (nested
      // AUXILIARY, where SSBO is the default kind) just need
      // `ACCESS: "read_only"` to get the readonly qualifier on the
      // emitted block. Override-CSFs that write the buffer keep their
      // `ACCESS: "read_write"` declaration as-is.
      //
      // The shader-side access pattern `scene_counts.light_count` is
      // identical against UBO or SSBO declarations; std140 vs std430
      // layouts agree on a 4-uint struct (16 bytes, no padding either
      // way).
      //
      // Advanced users writing their own shaders MAY still declare
      // `TYPE: "uniform"` for `scene_counts` — the parser supports it
      // — but they're responsible for ensuring the target backend
      // supports the resulting non-dynamic UBO bind. Bundled shaders
      // avoid it so they work on every backend.
      if(!m_sceneCountsBuffer)
      {
        m_sceneCountsBuffer = rhi.newBuffer(
            QRhiBuffer::Static, QRhiBuffer::StorageBuffer,
            sizeof(SceneCountsUBO));
        m_sceneCountsBuffer->setName("ScenePreprocessor::scene_counts");
        m_sceneCountsBuffer->create();
        // Zero-fill: Vulkan doesn't initialise VkBuffer memory. Until
        // the first scene_counts upload (gated below on actual count
        // changes), shaders reading scene_counts.light_count etc. would
        // see device-memory garbage — wildly different per resize as the
        // freshly allocated buffer lands on a different memory page.
        // SceneCountsUBO is a POD-of-uint32 — the all-zeros pattern
        // matches its default-constructed state.
        RhiClearBuffer::clearBuffer(
            rhi, res, m_sceneCountsBuffer, 0, sizeof(SceneCountsUBO));
      }

      // Allocate the shadow_cascades UBO once (544 B, never grows). Lazy:
      // only materialise the buffer when a scene actually authors cascades
      // — the vast majority of scenes without shadow-receiving rasterizers
      // pay zero GPU memory for this path.
      if(!m_shadowCascadesBuffer)
      {
        m_shadowCascadesBuffer = rhi.newBuffer(
            QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
            sizeof(ShadowCascadesUBO));
        m_shadowCascadesBuffer->setName("ScenePreprocessor::shadow_cascades");
        m_shadowCascadesBuffer->create();
        // Zero-fill so a no-shadow-cascade scene reads cascade_count=0
        // (the shader's "skip shadow sampling" sentinel) instead of
        // device-memory garbage on the first frame after a fresh
        // RenderList. RhiClearBuffer auto-routes Dynamic UBOs through
        // chunked updateDynamicBuffer (cap 65535 B per call); 560 B
        // here fits in a single chunk.
        RhiClearBuffer::clearBuffer(
            rhi, res, m_shadowCascadesBuffer, 0, sizeof(ShadowCascadesUBO));
      }

      // Camera UBO upload moved to the top of update() so it runs every
      // frame, decoupled from the mesh-rebuild gate (motion vectors need
      // per-frame camera_prev refresh; see comment at the head of
      // update()). The QRhiBuffer pointer is allocated on first call
      // there, so by the time rebuildMDI runs below, m_camerasBuffer is
      // non-null and ready to be attached as an aux on the emitted
      // geometry — same contract as before.

      // Pack the MERGED scene_environment into our own Env arena slot.
      // merge_scenes composes contributions from every EnvironmentLoader
      // / CubemapLoader / future IBL-precompute producer field-by-field
      // via the `params_set` bitmask, so this->scene.state->environment
      // holds the final composed state. Individual producer Env slots
      // still get written by those producers (they're POSTing their
      // own contribution for any future consumer wanting per-producer
      // data), but the scene_environment binding goes to our slot.
      if(m_registry && m_envSlot.valid() && this->scene.state)
      {
        const auto& env = this->scene.state->environment;
        EnvParamsUBO gpu{};
        gpu.ambient[0] = env.ambient_color[0];
        gpu.ambient[1] = env.ambient_color[1];
        gpu.ambient[2] = env.ambient_color[2];
        gpu.ambient[3] = env.ambient_intensity;
        gpu.fog_color_density[0] = env.fog.color[0];
        gpu.fog_color_density[1] = env.fog.color[1];
        gpu.fog_color_density[2] = env.fog.color[2];
        gpu.fog_color_density[3] = env.fog.density;
        gpu.fog_range[0] = env.fog.start;
        gpu.fog_range[1] = env.fog.end;
        gpu.fog_range[2] = float(env.fog.mode);
        gpu.fog_range[3] = env.fog.enabled ? 1.f : 0.f;
        gpu.exposure_gamma[0] = env.exposure;
        gpu.exposure_gamma[1] = env.gamma;
        gpu.exposure_gamma[2] = 0.f;
        gpu.exposure_gamma[3] = 0.f;
        if(!m_envSlotSeeded
           || std::memcmp(&gpu, &m_lastEnvUpload, sizeof(EnvParamsUBO)) != 0)
        {
          m_registry->updateSlot(res, m_envSlot, &gpu, sizeof(gpu));
          m_lastEnvUpload = gpu;
          m_envSlotSeeded = true;
        }
      }

      // Upload this preprocessor's private world-transforms buffer.
      // Per-preprocessor (not a shared registry arena) because two
      // preprocessors consuming different filtered views of the same
      // source scene legitimately compute different world matrices
      // for the same scene_transform — a shared arena would have them
      // stomp. Layout: indexed by the RawTransform arena slot index
      // (not walk order). Consumer shaders / compute passes read
      // `world_transforms.data[slot_index]` for any light / particle /
      // effect that needs slot-addressable world-space composition.
      {
        auto& rhi = *renderer.state.rhi;
        // Size to the full RawTransform arena capacity — sparse, but
        // bounded (16384 slots × 64 B = 1 MiB). Slot-indexed lookup
        // gives O(1) addressing without a per-frame translation table.
        const uint32_t xform_slot_count
            = renderer.registry().arenaSlotCount(
                GpuResourceRegistry::Arena::RawTransform);
        const int64_t want_bytes
            = (int64_t)xform_slot_count * (int64_t)sizeof(WorldTransformMat4);
        if(!m_worldTransformsBuffer || m_worldTransformsCap < want_bytes)
        {
          if(m_worldTransformsBuffer)
            renderer.releaseBuffer(m_worldTransformsBuffer);
          if(m_worldTransformsPrevBuffer)
            renderer.releaseBuffer(m_worldTransformsPrevBuffer);
          // QRhi forbids Dynamic + StorageBuffer — the SSBO path is
          // host-coherent differently from a Dynamic UBO's per-frame
          // rotation. Static + uploadStaticBuffer is the correct pair.
          m_worldTransformsBuffer = rhi.newBuffer(
              QRhiBuffer::Static, QRhiBuffer::StorageBuffer, (quint32)want_bytes);
          m_worldTransformsBuffer->setName("ScenePreprocessor::world_transforms");
          m_worldTransformsBuffer->create();
          // Prev buffer: same shape as current, sampled alongside it
          // as the `world_transforms_prev` aux for motion-vector /
          // TAA / reprojection shaders. Populated each frame by a
          // single GPU-side copyBuffer in runInitialPasses — see
          // m_worldTransformsPrevBuffer doc for the deferred-write
          // ordering that keeps the copy reading frame-N-1 data.
          m_worldTransformsPrevBuffer = rhi.newBuffer(
              QRhiBuffer::Static, QRhiBuffer::StorageBuffer, (quint32)want_bytes);
          m_worldTransformsPrevBuffer->setName(
              "ScenePreprocessor::world_transforms_prev");
          m_worldTransformsPrevBuffer->create();
          // Zero-fill both buffers. world_transforms is sparse —
          // only slots used by actual scene_transforms get written,
          // unused arena slots stay at their initial value. After a
          // fresh RenderList (resize), Vulkan hands us a VkBuffer with
          // device-memory garbage; any consumer indexing
          // world_transforms.data[L.transform_slot] for a slot the
          // producer hasn't populated reads garbage. Lights end up
          // with non-deterministic world positions per resize → the
          // user's "wildly different lighting on every resize"
          // symptom.
          //
          // _prev: the runInitialPasses copyBuffer(current → prev) on
          // the first post-resize frame would otherwise propagate the
          // current buffer's garbage into prev for any shader sampling
          // world_transforms_prev.
          //
          // RhiClearBuffer's batch variant pulls from the thread-local
          // zero pool — both 1 MiB clears reuse the same backing
          // vector (no per-buffer allocation).
          RhiClearBuffer::clearBuffer(
              rhi, res, m_worldTransformsBuffer, 0, (quint32)want_bytes);
          RhiClearBuffer::clearBuffer(
              rhi, res, m_worldTransformsPrevBuffer, 0, (quint32)want_bytes);
          m_worldTransformsCap = want_bytes;
        }
        // Sparse upload: one small write per scene_transform. Typical
        // scene has 1-50 transforms, so this is cheaper than packing
        // into a contiguous staging buffer. The arena-slot offsets
        // naturally cluster at the low indices (free-list LIFO stack
        // pops 0, 1, 2, … first) so uploads are cache-friendly.
        //
        // The actual uploadStaticBuffer is DEFERRED to runInitialPasses
        // so the prev-snapshot copyBuffer (which runs ahead of the
        // submitted writes) reads frame N-1 contents of current. Here
        // we just stash (slot, matrix) pairs; runInitialPasses drains
        // the list into the post-snapshot resource batch.
        m_pendingWorldXformWrites.clear();
        m_pendingWorldXformWrites.reserve(fs.worldTransforms.size());
        for(const auto& wt : fs.worldTransforms)
        {
          WorldTransformMat4 m;
          writeMat4(m.m, wt.world);
          m_pendingWorldXformWrites.emplace_back(wt.transform_slot, m);
        }
      }

      // Pack per-draw data once (cheap — just struct copy per draw).
      // `pd.material_index` is the Material-arena slot index
      // resolved by arenaSlotForMaterial(); shaders read
      // `scene_materials.entries[material_index]` directly against the
      // registry's Material arena. rebuildMDI() uses the same helper
      // on the full-rebuild path so the encoding is consistent.
      //
      // `pd.transform_slot` + `pd.skeleton_offset` + per_draw_bounds are
      // packed in lockstep with the other fields; fast path stays cheap
      // (one struct copy + one aabb copy per draw) and keeps the per_draw_bounds
      // sidecar in sync with per_draws for downstream culling CSFs.
      std::vector<uint32_t> fastSkinJointOffsets;
      fastSkinJointOffsets.reserve(fs.skins.size());
      {
        uint32_t running = 0;
        for(const auto& sk : fs.skins)
        {
          fastSkinJointOffsets.push_back(running);
          running += (uint32_t)sk.joint_matrices.size();
        }
      }

      std::vector<PerDrawGPU> freshPerDraws;
      std::vector<PerDrawBoundsGPU> freshPerDrawBounds;
      freshPerDraws.reserve(fs.draws.size());
      freshPerDrawBounds.reserve(fs.draws.size());
      for(const auto& dc : fs.draws)
      {
        // Mirror emitDraw's skip predicate exactly: a draw with
        // no usable positions, or with GPU-backed indices, is dropped by
        // rebuildMDI and therefore occupies NO per_draws slot. Filtering the
        // fast-path mirror only by `vertices > 0` would keep such draws and
        // shift every following slot, so diffUpload would write a draw's
        // model matrix into its neighbour's GPU slot.
        if(!dc.mesh || dc.mesh->vertices <= 0 || !m_registry)
          continue;
        if(!meshEmitsDraw(*dc.mesh))
          continue;
        PerDrawGPU pd{};
        writeMat4(pd.model, dc.worldTransform);
        QMatrix4x4 nm = dc.worldTransform.inverted().transposed();
        nm.setColumn(3, QVector4D(0, 0, 0, 1));
        nm.setRow(3, QVector4D(0, 0, 0, 1));
        writeMat4(pd.normal, nm);
        pd.material_index = arenaSlotForMaterial(dc.material.get());
        // tag_hash still keyed on the scene-material index (CPU-only
        // per-pass filter — not shader-visible as material identity).
        pd.tag_hash
            = (dc.materialIndex >= 0
               && (std::size_t)dc.materialIndex < materialTagHashes.size())
                ? materialTagHashes[dc.materialIndex]
                : 0u;
        pd.transform_slot = dc.transform_slot;
        pd.skeleton_offset
            = (dc.skinIndex >= 0
               && (std::size_t)dc.skinIndex < fastSkinJointOffsets.size())
                  ? fastSkinJointOffsets[dc.skinIndex]
                  : 0xFFFFFFFFu;
        freshPerDraws.push_back(pd);
        freshPerDrawBounds.push_back(packBounds(dc.local_bounds));
      }

      // Mesh fingerprint: the sequence of DrawCall::stable_id's — the
      // addresses of the source mesh_primitives (or legacy ossia::geometry
      // entries) that back each draw. Those addresses are invariant across
      // frames as long as the mesh_component shared_ptrs and their
      // primitives vectors don't change; walking the same scene tree twice
      // thus produces identical fingerprints and we can skip the full
      // vertex/index rebuild. (Contrast: `dc.mesh` is a fresh
      // primitiveToGeometry() wrapper pointer that differs every frame.)
      //
      // We also mix in the upstream GPU-resident attribute buffer handles
      // (positions/normals/texcoords/tangents). `m_pendingGpuCopies` holds
      // raw QRhiBuffer* captured in queueSlabCopy at rebuildMDI time and
      // re-issued every frame from runInitialPasses; if an upstream node
      // rebuilds its QRhiBuffer (CSF compute pipeline rebuild, Instancer
      // prototype swap, GPU mesh-handle pool churn) while the source
      // mesh_primitive address stays identical, the fast path would skip
      // rebuildMDI and the queue would re-issue copies from a freed
      // QRhiBuffer*. Including the upstream buffer pointers here makes any
      // such swap force a full rebuild → fresh op.src in the queue.
      std::vector<uint64_t> freshMeshFingerprint;
      freshMeshFingerprint.reserve(fs.draws.size() * 5);
      for(const auto& dc : fs.draws)
      {
        if(dc.mesh && dc.mesh->vertices > 0 && dc.stable_id)
        {
          freshMeshFingerprint.push_back(dc.stable_id);
          // Mix one entry per attribute: upstream QRhiBuffer* identity (or
          // 0 when the attribute is CPU-sourced / missing). A swap from
          // CPU→GPU sourcing or a buffer pointer change → fingerprint
          // mismatch → rebuildMDI repopulates m_pendingGpuCopies.
          auto bufId = [&](ossia::attribute_semantic sem) -> uint64_t {
            const auto v = extractGpuAttribute(*dc.mesh, sem);
            return reinterpret_cast<uintptr_t>(v.buf);
          };
          freshMeshFingerprint.push_back(
              bufId(ossia::attribute_semantic::position));
          freshMeshFingerprint.push_back(
              bufId(ossia::attribute_semantic::normal));
          freshMeshFingerprint.push_back(
              bufId(ossia::attribute_semantic::texcoord0));
          freshMeshFingerprint.push_back(
              bufId(ossia::attribute_semantic::tangent));
        }
      }

      // Cloud fingerprint: rebuildPrimitiveClouds is only
      // invoked on the full-rebuild branch, so any change to the primitive
      // cloud set must mismatch this fingerprint to force that branch. We
      // hash the same fields the function's internal per-bucket fingerprint
      // and bucket geometry depend on — raw_data identity + content version,
      // primitive_count, transform_slot, the world matrix (drives
      // CloudMetaGPU.model + AABBs), and the bucket key derived from
      // format_id — so added / removed / moved / re-uploaded clouds all flip
      // it. Count is mixed first so a pure add/remove is always detected.
      uint64_t freshCloudFingerprint = 0;
      ossia::hash_combine(
          freshCloudFingerprint, (uint64_t)fs.primitive_clouds.size());
      for(const auto& d : fs.primitive_clouds)
      {
        if(!d.cloud)
        {
          ossia::hash_combine(freshCloudFingerprint, (uint64_t)0);
          continue;
        }
        // Bucket key (mirrors rebuildPrimitiveClouds): hash(format_id), or
        // the cloud pointer when format_id is empty.
        const uint64_t bucket_key
            = !d.cloud->format_id.empty()
                  ? (uint64_t)(uint32_t)ossia::hash_string(d.cloud->format_id)
                  : (uint64_t)(uintptr_t)d.cloud.get();
        ossia::hash_combine(freshCloudFingerprint, bucket_key);

        const auto* raw = d.cloud->raw_data.get();
        ossia::hash_combine(freshCloudFingerprint, (uint64_t)(uintptr_t)raw);
        const uint64_t content_id
            = raw ? (raw->content_hash != 0 ? raw->content_hash
                                            : (uint64_t)raw->dirty_index)
                  : 0u;
        ossia::hash_combine(freshCloudFingerprint, content_id);
        ossia::hash_combine(
            freshCloudFingerprint, (uint64_t)d.cloud->primitive_count);
        ossia::hash_combine(
            freshCloudFingerprint, (uint64_t)d.transform_slot);
        ossia::hash_combine(
            freshCloudFingerprint,
            ossia::hash_bytes(d.worldTransform.constData(), 64));
      }

      // Pack per-material UV transforms (KHR_texture_transform) and
      // material extensions. Both buffers are read by the shader as
      // `entries[pd.material_index]` where pd.material_index is the
      // Material ARENA SLOT INDEX (parallel to `scene_materials`,
      // which IS the registry's Material arena). The buffers therefore
      // must also be arena-slot-indexed, not fs.materials-indexed —
      // otherwise a 1-material scene whose loader-material lands at
      // arena slot 1 reads entries[1] which is OUT OF BOUNDS, returning
      // zeros, collapsing every UV transform to (0,0) scale → all
      // textures sample pixel (0,0) → uniform color (the "solid gray
      // DamagedHelmet" symptom).
      std::vector<MaterialUVTransformGPU> freshMaterialUVTransforms(
          arenaSlotEntries);
      std::vector<MaterialExtensionsGPU> freshMaterialExtensions(
          arenaSlotEntries);
      if(this->scene.state && this->scene.state->materials)
      {
        const auto& mats = *this->scene.state->materials;
        auto pack_xform = [](float* dst_offset_scale, float* dst_rot,
                             const ossia::texture_ref& tr) {
          dst_offset_scale[0] = tr.uv_transform.offset[0];
          dst_offset_scale[1] = tr.uv_transform.offset[1];
          dst_offset_scale[2] = tr.uv_transform.scale[0];
          dst_offset_scale[3] = tr.uv_transform.scale[1];
          *dst_rot = tr.uv_transform.rotation;
        };
        for(std::size_t i = 0; i < mats.size(); ++i)
        {
          if(!mats[i])
            continue;
          const uint32_t slot = arenaSlotForMaterial(mats[i].get());
          if(slot >= arenaSlotEntries)
            continue;
          auto& g = freshMaterialUVTransforms[slot];
          pack_xform(g.bc_offset_scale,     &g.rotations0[0], mats[i]->base_color_texture);
          pack_xform(g.mr_offset_scale,     &g.rotations0[1], mats[i]->metallic_roughness_texture);
          pack_xform(g.normal_offset_scale, &g.rotations0[2], mats[i]->normal_texture);
          pack_xform(g.em_offset_scale,     &g.rotations0[3], mats[i]->emissive_texture);
          pack_xform(g.occ_offset_scale,    &g.rotations1[0], mats[i]->occlusion_texture);

          // Material extensions are already packed by flattenScene at
          // fs.material_extensions[i]; copy into the arena-slot index.
          if(i < fs.material_extensions.size())
            freshMaterialExtensions[slot] = fs.material_extensions[i];
        }
      }

      const bool meshesUnchanged
          = (freshMeshFingerprint == m_cachedMeshFingerprint)
            && m_outputSpec.meshes
            // If any aux buffer was just reallocated we need to republish
            // the output geometry so downstream picks up the new pointers.
            // rebuildMDI does this cleanly by building a fresh geometry
            // with wrapGpu() wrappers over the current buffer pointers.
            && !auxBuffersChanged
            // Cloud set unchanged: rebuildPrimitiveClouds only
            // runs on the full-rebuild branch and re-appends its bucket
            // geometries onto the freshly rebuilt mesh list, so any cloud
            // add / remove / move / re-upload must drop us off the fast path.
            && (freshCloudFingerprint == m_cachedCloudFingerprint)
            // The fast path's freshPerDraws / freshMeshFingerprint cover
            // fs.draws ONLY. fs.instances cmds (their world transforms,
            // instance counts, prototype identities, per-instance
            // GPU-buffer copies) are processed exclusively inside
            // rebuildMDI(); skipping it means Instancer control changes
            // and per-particle-data updates from upstream CSF compute
            // pipelines never reach the GPU. Force the full rebuild
            // path whenever any instance group is present.
            && fs.instances.empty();

      if(meshesUnchanged)
      {
        // Fast path: only diff-upload the small scene-level SSBOs. The
        // big vertex/index/indirect buffers are left alone, and
        // m_outputSpec.meshes is kept as the same shared_ptr (so
        // NodeRenderer::process on the downstream side sees
        // `this->geometry == v` and doesn't even flag geometryChanged).
        // scene_lights is the RawLight arena; producers keep it fresh
        // in their own update() hooks. Only the compact indices list
        // needs a diff upload.
        diffUpload(res, m_lightIndicesBuffer, m_cachedLightIndices,
                   freshLightIndices);
        // scene_materials: producer + loader-material upload pass
        // above already pushed MaterialGPU bytes into the Material
        // arena. Nothing to diff-upload here.
        diffUpload(res, m_materialsExtBuffer, m_cachedMaterialExt,
                   freshMaterialExtensions);
        diffUpload(res, m_materialUVTransformsBuffer,
                   m_cachedMaterialUVTransforms, freshMaterialUVTransforms);
        diffUpload(res, m_mdi.per_draws,   m_cachedPerDraws,  freshPerDraws);
        // per_draw_bounds is static across a frame (local-space AABB,
        // never changes per-frame for the same topology) — on the fast
        // path the mirror and fresh arrays match element-for-element and
        // diffUpload short-circuits to zero uploads. Kept in the fast
        // path for robustness (e.g. a material-swap flow that re-picks
        // a primitive variant with different bounds under the hood).
        diffUpload(res, m_mdi.per_draw_bounds, m_cachedPerDrawBounds,
                   freshPerDrawBounds);
      }
      else
      {
        // Something structural changed (meshes added/removed/reordered).
        // Fall back to the full rebuild path. scene_lights arena bytes
        // are maintained by each Light producer's update() hook — we
        // only push the compacted indices list here.
        if(!freshLightIndices.empty())
          res.uploadStaticBuffer(
              m_lightIndicesBuffer, 0,
              freshLightIndices.size() * sizeof(uint32_t),
              freshLightIndices.data());
        // scene_materials: arena upload already happened above (see
        // the "loader-material arena slot upload" block).
        if(!freshMaterialExtensions.empty())
          res.uploadStaticBuffer(
              m_materialsExtBuffer, 0,
              freshMaterialExtensions.size() * sizeof(MaterialExtensionsGPU),
              freshMaterialExtensions.data());
        if(!freshMaterialUVTransforms.empty())
          res.uploadStaticBuffer(
              m_materialUVTransformsBuffer, 0,
              freshMaterialUVTransforms.size() * sizeof(MaterialUVTransformGPU),
              freshMaterialUVTransforms.data());

        rebuildMDI(renderer, res, fs, materialTagHashes);
        rebuildPrimitiveClouds(renderer, res, fs);

        // Seed the CPU mirrors from the fresh data so subsequent frames
        // can take the fast path via diffUpload.
        m_cachedMeshFingerprint = std::move(freshMeshFingerprint);
        m_cachedCloudFingerprint = freshCloudFingerprint;
        m_cachedLightIndices = std::move(freshLightIndices);
        m_cachedMaterialExt = std::move(freshMaterialExtensions);
        m_cachedMaterialUVTransforms = std::move(freshMaterialUVTransforms);
        // m_cachedPerDraws / m_cachedPerDrawBounds are NOT seeded here:
        // rebuildMDI() already assigned them from acc.perDraws (the
        // actually-emitted set, after emitDraw's skip predicate), so the
        // mirror matches the GPU per_draws layout slot-for-slot. Seeding
        // from freshPerDraws (filtered only by vertices>0) would reintroduce
        // the slot divergence whenever a draw was skipped.
      }

      // Camera + Env UBOs are packed above, before rebuildMDI, so that the
      // geometry's auxiliary entries reference valid buffer pointers. The
      // pre-sized capacity keeps those pointers stable across parameter
      // changes on the fast path (no re-rebuild needed).

      // scene_counts SSBO: tell shaders the authoritative N for each
      // SSBO (so they don't rely on `.length()` which reports buffer
      // capacity and includes zeroed tail slots when counts shrank).
      // Uploaded only when a count actually changed.
      // light_count is the arena-addressable subset (matches
      // m_cachedLightIndices / scene_light_indices). Post 28b-shader
      // flip: shaders iterate via the indices buffer, so this count
      // drives that loop.
      SceneCountsUBO sc{
          (uint32_t)m_cachedLightIndices.size(),
          (uint32_t)fs.materials.size(),
          (uint32_t)m_mdi.drawCount,
          0u};
      if(std::memcmp(&sc, &m_cachedSceneCounts, sizeof(sc)) != 0)
      {
        // Allocation is Static + StorageBuffer on every backend, so the
        // upload always goes through uploadStaticBuffer — at 16 bytes
        // the difference vs updateDynamicBuffer is negligible anyway.
        res.uploadStaticBuffer(m_sceneCountsBuffer, 0, sizeof(sc), &sc);
        m_cachedSceneCounts = sc;
      }

      // shadow_cascades UBO: populated from scene_state.shadow_cascades
      // (authored upstream by Threedim::ShadowCascadeSetup). Straight
      // struct copy — the CPU-side shadow_cascades_info layout mirrors
      // the GPU ShadowCascadesUBO field-for-field: light_view_proj[8]
      // (column-major mat4 array), split_view_depths[9] compacted into
      // cascade_split_distances[8], cascade_count (uint32). Diff-uploaded
      // against the cached snapshot so frames without topology / camera
      // changes cost zero UBO bytes.
      //
      // When no upstream authored cascades (the field defaults to
      // cascade_count=0), we still publish the UBO with zero count so
      // downstream shaders that declare `shadow_cascades` as INPUT have
      // a valid binding and fall through their own "cascade_count == 0
      // → skip shadow sampling" guard.
      ShadowCascadesUBO sh{};
      if(this->scene.state)
      {
        const auto& src = this->scene.state->shadow_cascades;
        sh.cascade_count
            = std::min<uint32_t>(src.cascade_count,
                                 ossia::shadow_cascades_info::max_cascades);
        std::memcpy(
            sh.light_view_proj, src.light_view_proj,
            sizeof(sh.light_view_proj));
        // Shaders sample cascade_split_distances[k] for cascade picks;
        // slot k is the far-plane Z of cascade k (view-space).
        // CPU-side stores count+1 BOUNDARIES in split_view_depths[]:
        // entry k is the near plane of cascade k, entry count is the
        // scene far plane. The UBO contract wants slot k = FAR plane of
        // cascade k, i.e. boundary k+1 — copying boundary k instead
        // shifts every split by one slice (slot 0 would hold the camera
        // near plane and shaders would assign fragments to the wrong
        // cascade), and drops the real far distance at count == 8.
        const uint32_t kLayoutSlots = ossia::shadow_cascades_info::max_cascades; // 8
        for(uint32_t k = 0; k < kLayoutSlots; ++k)
        {
          sh.cascade_split_distances[k]
              = (k < sh.cascade_count)
                    ? src.split_view_depths[k + 1]
                    : 0.f;
        }
      }
      if(!m_shadowCascadesSeeded
         || std::memcmp(&sh, &m_cachedShadowCascades,
                        sizeof(ShadowCascadesUBO)) != 0)
      {
        res.updateDynamicBuffer(
            m_shadowCascadesBuffer, 0, sizeof(sh), &sh);
        m_cachedShadowCascades = sh;
        m_shadowCascadesSeeded = true;
      }

      // Instance components are now handled directly inside rebuildMDI
      // (above) — every fs.instances entry rides through the same
      // unified indirect-cmd batch as fs.draws. No separate sub-mesh
      // emission step is needed.
    }

    m_cachedSceneState = this->scene.state.get();
    m_cachedVersion = this->scene.state ? this->scene.state->version : -1;
    this->sceneChanged = false;

    // Skybox + texture-channel changes propagate through the geometry's
    // auxiliary_texture entries on Geometry Out — consumer shaders
    // re-resolve pointers per frame via try_bind_texture_from_geometry.
    // This also bumps mesh identity on channel-array realloc so
    // downstream's update() reruns without missing a rebind.
  }

  // Resolve an MDI attribute enum to the matching arena stream buffer
  // (streams moved from MDIState to the registry).
  QRhiBuffer* mdiBufferFor(MdiAttr a) const noexcept
  {
    if(!m_registry)
      return nullptr;
    using Stream = GpuResourceRegistry::MeshStream;
    switch(a)
    {
      case MdiAttr::Positions: return m_registry->meshStreamBuffer(Stream::Positions);
      case MdiAttr::Normals:   return m_registry->meshStreamBuffer(Stream::Normals);
      case MdiAttr::Texcoords: return m_registry->meshStreamBuffer(Stream::Texcoords);
      case MdiAttr::Tangents:  return m_registry->meshStreamBuffer(Stream::Tangents);
    }
    return nullptr;
  }

  // Issue every pending GPU→GPU copy queued during update(). Called every
  // frame in runInitialPasses regardless of whether update() rebuilt the
  // accumulator — upstream GPU buffer CONTENTS change every frame (CSF
  // compute writes) while the buffer HANDLES + MDI offsets stay stable as
  // long as no draw-topology change occurred. The queue is rebuilt (via
  // clear + repopulate at the top of the accumulator loop) only when the
  // scene actually changed; otherwise the same ops fire with fresh data.
  //
  // Stride-equal-to-element copies collapse to a single copyBuffer;
  // vec4→vec3-style strided copies fall back to a per-vertex loop (one
  // copyBuffer per vertex — acceptable for typical CSF point clouds of
  // a few thousand vertices).
  void issuePendingGpuCopies(RenderList& renderer, QRhiCommandBuffer& cb)
  {
    if(m_pendingGpuCopies.empty())
      return;
    auto* rhi = renderer.state.rhi;
    if(!rhi)
      return;
    cb.beginExternal();
    // One compute→transfer barrier for the whole batch instead of one per
    // copy call — eliminates N−1 redundant pipeline stalls on Vulkan.
    score::gfx::beginBufferCopyBarrier(*rhi, cb);
    // Scratch reused across ops — avoids reallocating for each strided op.
    std::vector<score::gfx::BufferCopyRegion> regions;
    for(const auto& op : m_pendingGpuCopies)
    {
      // Explicit dst wins over the mesh-stream lookup — used by the
      // unified-MDI per-instance concat copies (translations / colors)
      // which target preprocessor-owned buffers, not arena streams.
      QRhiBuffer* dst = op.dst ? op.dst : mdiBufferFor(op.attr);
      if(!op.src || !dst)
        continue;
      if(op.src_stride == 0 || op.src_stride == op.element_size)
      {
        // Tight source layout — one copy, no per-call barrier (batched).
        score::gfx::copyBuffer(
            *rhi, cb, op.src, dst,
            op.vertex_count * op.element_size,
            op.src_offset, op.dst_offset,
            score::gfx::BufferCopyBarrier::None);
      }
      else
      {
        // Strided source — src slot size differs from MDI slot size.
        // Per-vertex copy of min(src_stride, element_size) bytes: the
        // overlap between the two layouts (e.g. tight vec3 src (12 B) →
        // padded-vec4 MDI slot (16 B) → copy the 12 B of real data into
        // each slot's low bytes; zero-fill from uploadStaticBuffer covers
        // the trailing padding).
        const int per_vertex
            = std::min(op.src_stride, op.element_size);
        regions.clear();
        regions.reserve(op.vertex_count);
        for(int v = 0; v < op.vertex_count; ++v)
        {
          regions.push_back(
              {op.src_offset + v * op.src_stride,
               op.dst_offset + v * op.element_size,
               per_vertex});
        }
        score::gfx::copyBufferRegions(
            *rhi, cb, op.src, dst, regions.data(), (int)regions.size(),
            score::gfx::BufferCopyBarrier::None);
      }
    }
    score::gfx::endBufferCopyBarrier(*rhi, cb);
    cb.endExternal();
    // Intentionally NOT clearing m_pendingGpuCopies here — the list is
    // owned by the accumulator and persists across cache-hit frames so
    // updates to upstream buffer contents keep flowing through.
  }

  // Push the produced geometry_spec to the downstream renderer's input port.
  void runInitialPasses(
      RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, Edge& edge) override
  {
    // Debug marker for capture-tool readability.
    commands.debugMarkBegin(QByteArrayLiteral("ScenePreprocessor"));
    struct MarkEnd
    {
      QRhiCommandBuffer* c;
      ~MarkEnd() { c->debugMarkEnd(); }
    } _me{&commands};

    // GPU→GPU copies run before the geometry_spec hand-off so the
    // destination MDI buffers are populated by the time the downstream
    // rasterizer starts reading them. Frame-gated — the
    // copies target shared MDI buffers, so one batch per frame serves every
    // consumer; without the gate a node feeding K downstreams issues K
    // identical copy batches. Same renderer.frame token discipline as the
    // world-transforms snapshot below.
    if(m_lastGpuCopiesFrame != renderer.frame)
    {
      issuePendingGpuCopies(renderer, commands);
      m_lastGpuCopiesFrame = renderer.frame;
    }

    // Snapshot last frame's world_transforms into the prev buffer via
    // a pure GPU copy, then apply this frame's per-slot writes via the
    // (post-snapshot) resource-update batch. The ordering invariant is:
    //
    //   commands stream      : ... [updateBatch_N applied] [copyBuffer current→prev] ...
    //   res (next batch)     :                                                  [uploadStaticBuffer per slot]
    //   RenderList submits   :                                                  ^ next iteration
    //
    // So the copy reads m_worldTransformsBuffer at its frame-N-1
    // contents (no frame-N writes have hit it yet — those are queued
    // in `*res`, applied AFTER this function returns), and the next
    // beginPass sees current = frame N + prev = frame N-1.
    //
    // Gate on renderer.frame because runInitialPasses fires once per
    // outgoing edge: without the guard a node feeding K downstreams
    // would queue K back-to-back current→prev copies (the second-and-
    // later seeing prev = current = frame N) and would re-upload the
    // pending writes K times. Within one frame renderer.frame is
    // stable; across frames it advances monotonically, so the
    // mismatch correctly discriminates "first call this frame".
    //
    // Fire EVERY frame (not gated on pending non-empty): for a static
    // scene the per-frame copy is what KEEPS prev == current, so
    // motion vectors stay zero. A previous attempt to skip when pending
    // was empty froze prev at the value from the last animated frame
    // and produced ghost motion on idle scenes.
    //
    // The previous CB-pointer discriminator was broken: every QRhi
    // backend's QRhiSwapChain::currentFrameCommandBuffer returns the
    // address of a single by-value cbWrapper member, so the pointer is
    // constant across frames and the gate fired exactly once per
    // swapchain lifetime — freezing world_transforms / _prev at frame
    // 0 (motion vectors / TAA / reprojection silently broken).
    //
    // Frame 0 sees prev=zeroes → first-frame MV is large; consumer
    // shaders handle that via frame-index / temporal accumulation.
    // Auto barrier covers the compute↔transfer hazards around the copy.
    if(m_worldTransformsBuffer && m_worldTransformsPrevBuffer
       && m_worldTransformsCap > 0
       && m_lastSnapshotFrame != renderer.frame)
    {
      commands.beginExternal();
      copyBuffer(
          *renderer.state.rhi, commands,
          m_worldTransformsBuffer, m_worldTransformsPrevBuffer,
          (int)m_worldTransformsCap);
      commands.endExternal();

      // Drain deferred per-slot writes into the next resource batch
      // (`res` — distinct from the batch already submitted in
      // RenderList::renderInternal before this function ran). The
      // batch is submitted later, AFTER the copy above has executed.
      if(res && !m_pendingWorldXformWrites.empty())
      {
        for(const auto& [slot, m] : m_pendingWorldXformWrites)
        {
          const uint32_t byte_offset
              = slot * (uint32_t)sizeof(WorldTransformMat4);
          res->uploadStaticBuffer(
              m_worldTransformsBuffer, byte_offset,
              (quint32)sizeof(WorldTransformMat4), &m);
        }
        m_pendingWorldXformWrites.clear();
      }

      m_lastSnapshotFrame = renderer.frame;
    }

    auto* src = edge.source;
    const int src_port_idx = src && src->node
        ? int(std::find(src->node->output.begin(), src->node->output.end(), src)
              - src->node->output.begin())
        : -1;

    // Only the Geometry output (port 0) pushes a geometry_spec — it's
    // the sole remaining output. Guard kept for robustness in case the
    // port layout is extended again.
    if(src_port_idx != 0)
      return;
    if(!m_outputSpec.meshes)
      return;

    auto* sink = edge.sink;
    if(!sink || !sink->node)
      return;

    auto rn_it = sink->node->renderedNodes.find(&renderer);
    if(rn_it == sink->node->renderedNodes.end())
      return;

    auto it = std::find(sink->node->input.begin(), sink->node->input.end(), sink);
    if(it == sink->node->input.end())
      return;

    int port_idx = (int)(it - sink->node->input.begin());
    BUFTRACE() << "ScenePreprocessor → sink_node=" << sink->node->nodeId
               << " port=" << port_idx
               << " mdi_indices="
               << (void*)(m_registry ? m_registry->meshStreamBuffer(
                       GpuResourceRegistry::MeshStream::Indices) : nullptr)
               << " mdi_positions="
               << (void*)(m_registry ? m_registry->meshStreamBuffer(
                       GpuResourceRegistry::MeshStream::Positions) : nullptr)
               << " mdi_drawCmds=" << (void*)m_mdi.indirect_draw_cmds
               << " mdi_drawCount=" << (quint32)m_mdi.drawCount;
    rn_it->second->process(port_idx, m_outputSpec, edge.source);
  }

  void runRenderPass(RenderList&, QRhiCommandBuffer&, Edge&) override { }

  // Data-only renderer — no per-edge GPU pass state to release. All GPU
  // resources live on the renderer itself (buffers, textures) and are
  // dropped in releaseState; nothing is keyed by output edge.
  void removeOutputPass(RenderList&, Edge&) override { }
};

ScenePreprocessorNode::ScenePreprocessorNode()
{
  // Port 0: Scene input (carries scene_spec — carries EVERYTHING,
  // including the environment and its skybox/IBL textures).
  input.push_back(new Port{this, {}, Types::Scene, {}});

  // Single outlet: geometry (concatenated MDI geometry). Scene-wide
  // UBOs/SSBOs (per_draws, indirect_draw_cmds, scene_lights,
  // scene_materials, scene_counts, camera, env) ride along as
  // auxiliary_buffer entries; per-channel material texture arrays
  // (base_color_array, metal_rough_array, normal_array, emissive_array)
  // and the environment skybox ride along as auxiliary_texture entries.
  // Consumer shaders bind them all by name via
  // try_bind_from_geometry / try_bind_texture_from_geometry.
  output.push_back(new Port{this, {}, Types::Geometry, {}});
}

ScenePreprocessorNode::~ScenePreprocessorNode() = default;

NodeRenderer* ScenePreprocessorNode::createRenderer(RenderList& /*r*/) const noexcept
{
  return new RenderedScenePreprocessorNode{*this};
}

}
