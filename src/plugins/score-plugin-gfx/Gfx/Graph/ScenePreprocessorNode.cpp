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

// std430 layout matching the `per_draw` AUXILIARY block in the preset
// rasterizer shaders. tag_hash is rapidhash(material.tag) truncated to 32 bits.
// transform_slot indexes world_transforms / world_transforms_prev;
// skeleton_offset indexes joint_matrices. 0xFFFFFFFF means "none" for both.
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

// Local-space AABB per draw, emitted as the `per_draw_bounds` SSBO alongside
// `per_draws` and indexed the same way. A mesh without bounds gets an infinite
// AABB so culling shaders leave the draw alone.
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

// Per-material per-channel UV transforms (KHR_texture_transform): 5 channels
// x (offset.xy + scale.xy) + rotations packed in 2 vec4 = 7 vec4 = 112 B.
// Channels match MaterialChannel: 0=BC, 1=MR, 2=Normal, 3=Em, 4=Occlusion.
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
// Slot numbering must stay in sync with MaterialExtensionsGPU::textureRefs[] in
// SceneGPUState.hpp and the shader-side switch in classic_pbr_openpbr.frag.
// Pool choice follows the format the texture carries: ChannelBaseColor for sRGB
// colour, ChannelMetalRough for linear scalars, ChannelNormal for tangent space.
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
  // Texture arrays are owned by GpuResourceRegistry, not by this node.

  const ScenePreprocessorNode& m_node;

  // Output owned GPU buffers (one set per flatten cycle). Sized to scene needs.
  // scene_light_indices SSBO: compact list of RawLight arena slot
  // indices for the current scene's live lights. Shader iterates
  // 0..scene_counts.light_count and reads
  // scene_lights.entries[scene_light_indices.data[i]].
  QRhiBuffer* m_lightIndicesBuffer{};
  int64_t m_lightIndicesCap{};
  std::vector<uint32_t> m_cachedLightIndices;
  // scene_materials is served by the Material arena directly
  // (registry.buffer(Arena::Material)) — no preprocessor-owned mirror.
  // MaterialExtensions is preprocessor-owned.
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

  // std140 counts UBO: shaders read scene_counts.* rather than SSBO .length(),
  // so growth-only capacity does not force them to iterate dead tail slots.
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

  // `shadow_cascades` aux UBO: light_view_proj[8], split distances and
  // cascade_count, authored upstream by ShadowCascadeSetup, diff-uploaded.
  // shadow_cascades.vert reads light_view_proj; its per-invocation
  // cascade_index lives in the separate `shadow_draw_cfg` UBO that the
  // depth-pass pipeline binds locally.
  QRhiBuffer* m_shadowCascadesBuffer{};
  ShadowCascadesUBO m_cachedShadowCascades{};
  bool m_shadowCascadesSeeded{false};

  // Per-camera std140 UBO array, first entry always the active camera. A scene
  // with no cameras publishes one default entry so the binding is never null.
  QRhiBuffer* m_camerasBuffer{};
  int64_t m_camerasCap{};
  std::vector<CameraUBOData> m_cachedCameras;

  // One-frame history for motion-vector reprojection, bound as `camera_prev`.
  // Seeded to the current camera on the first frame. Filled from
  // m_cachedCameras before m_camerasBuffer is overwritten.
  QRhiBuffer* m_camerasPrevBuffer{};

  // packAndUploadCameras must run once per frame, not once per outgoing edge:
  // the camera-prev semantics only hold on the first call. -1 = not yet run.
  int64_t m_lastCameraUploadFrame{-1};

  // Per-preprocessor world-transforms SSBO, one WorldTransformMat4 per
  // producer-authored scene_transform in walk order. Not a shared arena:
  // different filtered views of one scene yield different world matrices.
  QRhiBuffer* m_worldTransformsBuffer{};
  int64_t m_worldTransformsCap{0};

  // Previous-frame snapshot of m_worldTransformsBuffer, bound as
  // `world_transforms_prev`. update() stashes this frame's writes into
  // m_pendingWorldXformWrites; runInitialPasses then copies current -> prev on
  // the command buffer, where current still holds frame N-1, and only
  // afterwards drains the pending writes into the resource-update batch.
  // Static + StorageBuffer because QRhi forbids Dynamic + StorageBuffer.
  QRhiBuffer* m_worldTransformsPrevBuffer{};

  // Per-slot world-transform writes deferred from update() to
  // runInitialPasses so that the prev-snapshot copy captures frame
  // N-1 data before frame N's writes overwrite current. Drained once
  // per frame, gated by m_lastSnapshotFrame.
  std::vector<std::pair<uint32_t, WorldTransformMat4>>
      m_pendingWorldXformWrites;
  // Gates the prev-snapshot and pending-writes drain to once per frame;
  // runInitialPasses fires once per outgoing edge. Discriminated on
  // renderer.frame: a QRhiCommandBuffer pointer cannot serve here, since every
  // backend returns the address of one by-value member. Cleared on teardown.
  int64_t m_lastSnapshotFrame{-1};

  // Gates issuePendingGpuCopies to once per frame. Separate from
  // m_lastSnapshotFrame, which is only set when the world-transforms buffer
  // exists; this token gates the copies unconditionally. Cleared on teardown.
  int64_t m_lastGpuCopiesFrame{-1};

  // Environment params UBO holding the MERGED env: merge_scenes composes the
  // per-producer scene_environment contributions field by field, and consumers
  // binding `env` must see that result, not any one producer's slot.
  GpuResourceRegistry::Slot m_envSlot{};
  uint32_t m_env_aux_offset{0};
  // Cache the last uploaded EnvParamsUBO bytes so we can skip re-upload
  // when the merged environment content doesn't change frame-to-frame.
  EnvParamsUBO m_lastEnvUpload{};
  bool m_envSlotSeeded{false};

  // ─── MDI state ───────────────────────────────────────────────────────
  // Vertex/index streams live in the registry's MeshArenaManager. per_draws
  // and indirect_draw_cmds stay preprocessor-owned: they are tied to this
  // preprocessor's filtered view of the scene and are not shareable.
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
  // One entry per bucket_key = hash(format_id), or stable_id when format_id
  // is empty so each unformatted cloud gets its own bucket. A bucket holds
  // raw_splats (concatenated raw_data), cloud_meta (CloudMetaGPU[]),
  // cloud_id_lookup (uint per primitive -> cloud_meta index) and one
  // IndirectCmd {6, total_primitives, 0, 0, 0}.
  //
  // Buffers are growBuf-managed so downstream SRBs see pointer-stable
  // handles; a key that disappears is dropBuf'd in releaseStaleClouds().
  // CloudMetaGPU mirrors PerDrawGPU (model[16] + transform_slot) so a splat
  // CSF reads per-cloud TRS exactly as mesh shaders read per_draws[gl_DrawID].
  // bounds_min/max are the per-cloud world AABB, from walking the 8 corners
  // of cloud->bounds through worldTransform, for per-cloud frustum culling.
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
    // Per-frame content fingerprint over, per cloud in bucket order,
    // raw_data identity + content_hash + primitive_count + worldTransform
    // bytes + transform_slot. A match means the bucket's buffers are already
    // correct and the CPU concat + upload can be skipped. 0 forces upload.
    uint64_t content_fingerprint{};
  };
  ossia::flat_map<uint32_t, PrimitiveCloudBucketBuffers> m_primitiveCloudBuckets;
  uint64_t m_primitiveCloudFrame{0};

  // ─── Unified-MDI per-instance concat buffers ────────────────────────
  // Two arrays sized to K = (Σ regular_cmd_count + Σ instance_group_count),
  // one slot per (cmd, instance) pair, contiguous within a cmd. Each indirect
  // cmd sets firstInstance to its first slot, so per-instance VERTEX_INPUTs
  // step at the right offset on both the indirect and the CPU-fallback path
  // (every QRhi backend honours firstInstance).
  //
  // m_instAttribs is INTERLEAVED, 32 bytes a slot: a vec4-padded translation
  // at offset 0 (identity for regular meshes) then a vec4 color at offset 16
  // (identity (1,1,1,1) for regular meshes). They used to be two buffers on
  // two vertex bindings, which cost the geometry a ninth binding — one past
  // what Qt's D3D11 command buffer records. Both are per-instance vec4s
  // written by the same GPU copy pass, so one binding carries both.
  //
  // m_instDrawIds stays on its own: cmd-index of the owning draw, standing in
  // for gl_DrawID, which the CPU fallback does not provide, and for
  // gl_BaseInstance, which stops equalling drawID once instanceCount > 1. It
  // is a 4-byte uint diff-uploaded from a CPU mirror as one contiguous range,
  // which interleaving would turn into a per-slot scatter.
  static constexpr int kInstSlotStride = 32;
  static constexpr int kInstTranslationOffset = 0;
  static constexpr int kInstColorOffset = 16;
  QRhiBuffer* m_instAttribs{};
  QRhiBuffer* m_instDrawIds{};
  int64_t m_instAttribsCap{};
  int64_t m_instDrawIdsCap{};
  uint32_t m_instSlotsUsed{};

  // CPU mirror of the draw_ids stream so we can diff-upload + cheaply
  // pre-fill identity values for regular cmds. Translations / colors
  // are GPU-resident sources for instance groups (no CPU mirror —
  // copies are GPU→GPU); we pre-fill identity for regular slots
  // straight into the GPU buffer via uploadStaticBuffer.
  std::vector<uint32_t> m_cachedInstDrawIds;

  // Prototype stable-id fallback. Producers going through halp::geometry ->
  // legacy_geometry do not stamp mesh_primitive::stable_id, and without one
  // the slab arena allocates a fresh slab per frame until the OffsetAllocator
  // is exhausted. Mint an id keyed on the prototype's mesh_component pointer,
  // which is stable while the producer re-emits the same shared_ptr; a GC pass
  // at the end of update() evicts entries whose pointer no longer appears.
  ossia::hash_map<const ossia::mesh_component*, uint64_t> m_protoStableIds;

  // GPU->GPU copy ops collected during update()'s accumulator loop and issued
  // in runInitialPasses, the only place with a live command buffer. One op per
  // attribute of one draw whose source buffer is GPU-resident; the CPU
  // accumulator is zero-filled in its place to keep the MDI layout tight.
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
    int dst_stride{};    // 0 or element_size → tight; else the destination
                         // slot is wider than the element, as it is for the
                         // interleaved per-instance array where translation
                         // and color share a 32-byte slot
    int element_size{};  // BytesPerVertex for this attribute
    MdiAttr attr{};
  };
  std::vector<PendingGpuCopy> m_pendingGpuCopies;

  // Capacities (in bytes) of the two shared scene buffers — for growth-only.
  int64_t m_materialsExtCap{};

  // Per-channel material texture arrays are owned by GpuResourceRegistry and
  // shared across every preprocessor in the RenderList: layer assignment is
  // driven by asset identity (texture_source pointer), which is
  // view-independent, so every preprocessor computes the same mapping.
  //
  // Stashed at init() rather than fetched from renderer.registry() per call:
  // this is on the hot rebuild path.
  GpuResourceRegistry* m_registry{};

  // Snapshot of m_registry taken at release(), so a following init() can tell
  // "same registry" (relink, viewport resize: keep the caches) from a new one
  // (first init, OutputNode-replaced QRhi: wipe). Only read from init().
  GpuResourceRegistry* m_lastRegistry{};

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

  // Content fingerprint of the last decoded materials list: the raw
  // material_component pointers. merge_scenes concatenates those elements
  // without deep-copying, so they stay stable across frames even though the
  // enclosing shared_ptr<vector<>> is reallocated on every multi-producer
  // merge. Comparing by element identity keeps the texture cache warm across
  // multi-glTF scenes.
  std::vector<uint64_t> m_cachedMaterialsFingerprint;

  // Value computeDynamicSlotFingerprint() returned at the last full rebuild,
  // i.e. the dynamic-slot table the currently-published m_outputSpec.meshes'
  // auxiliary_textures and the currently-uploaded MaterialGPU::textureRefs
  // were built from. Compared against the fresh value every frame so a live
  // texture-handle reroute republishes on its own account instead of riding
  // on whatever unrelated buffer happened to grow in the same frame.
  uint64_t m_cachedDynamicSlotFingerprint{};

  // -- Granular invalidation state ------------------------------------------
  //
  // CPU mirrors of what is currently on the GPU for each small SSBO, plus a
  // fingerprint of the concatenated mesh list. Per frame: if the fingerprint
  // is unchanged, skip the vertex/index upload and keep m_outputSpec.meshes as
  // the same shared_ptr so downstream sees a stable geometry_spec; then diff
  // the mirrors and uploadStaticBuffer only the contiguous changed ranges.
  // Moving a light costs one 64-byte partial upload, moving an object one
  // PerDrawGPU (144 bytes). CPU cost ~sizeof(T) x count, tens of KB typically.
  //
  // m_cachedMeshFingerprint stores DrawCall::stable_id, the address of the
  // source mesh_primitive inside the stable mesh_component shared_ptr -- not
  // DrawCall::mesh, which points at a primitiveToGeometry() wrapper freshly
  // allocated on every flattenScene() call.
  std::vector<uint64_t> m_cachedMeshFingerprint;
  // Fingerprint of the primitive_cloud set. Clouds are not covered by
  // m_cachedMeshFingerprint, so they get their own gate on the fast path.
  // Covers the fields rebuildPrimitiveClouds' per-bucket fingerprint depends
  // on -- raw_data identity, primitive count, transform -- plus the bucket key
  // so additions and removals are detected.
  uint64_t m_cachedCloudFingerprint{};
  std::vector<MaterialExtensionsGPU> m_cachedMaterialExt;
  std::vector<PerDrawGPU> m_cachedPerDraws;
  // Mirror of the per_draw_bounds SSBO for diff-upload on the fast-path
  // (transforms/materials change but topology doesn't → tiny range
  // upload instead of full rewrite). Grow-only; same indexing as
  // m_cachedPerDraws.
  std::vector<PerDrawBoundsGPU> m_cachedPerDrawBounds;

  // Arena slots this preprocessor allocated for loader materials, i.e. those
  // entering scene_state.materials with raw_slot.size == 0. It acts as a
  // producer on their behalf: one Material slot each, MaterialGPU bytes
  // written, freed at release. Producer-authored materials keep their own.
  ossia::hash_map<
      const ossia::material_component*, GpuResourceRegistry::Slot>
      m_loaderMaterialSlots;

  // Accumulator sizes from the last full rebuildMDI, used to pre-reserve the
  // temporary vector capacity. Grow-only. Vertex/index stream sizes are the
  // arena OffsetAllocator's business; m_lastDrawCount pre-reserves
  // acc.perDraws and acc.indirectCmds.
  std::size_t m_lastDrawCount{};

  // Diff two CPU mirrors and partial-upload only the contiguous ranges where
  // fresh != cached, growing or shrinking the mirror to match. Returns true if
  // anything was uploaded. A shrunk tail is zero-filled on the GPU so stale
  // content cannot contribute.
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

  // Graph::incrementalEdgeUpdate creates fresh renderers and calls initState()
  // rather than init(). This preprocessor has no per-edge state, so both entry
  // points run the same setup.
  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    init(renderer, res);
  }

  void releaseState(RenderList& renderer) override
  {
    release(renderer);
  }

  // Reset every per-RenderList / per-registry cache field. Registry-allocated
  // slots (loader-material, env) are freed when freeRegistryResources is true:
  // pass true from release(), where m_registry is still valid, and false from
  // init(), where the prior registry may already be torn down and can only be
  // forgotten, not freed.
  //
  // QRhiBuffer-backed fields and their paired *Cap counters are not touched
  // here; they go through dropBuf / renderer.releaseBuffer in release().
  void clearAllCaches(bool freeRegistryResources, uint32_t current_frame = 0u)
  {
    if(freeRegistryResources && m_registry)
    {
      for(auto& [mat, slot] : m_loaderMaterialSlots)
        if(slot.valid())
          m_registry->free(slot);
      if(m_envSlot.valid())
        m_registry->free(m_envSlot);
      // Release the slabs keyed by the ids minted in resolvePrototypeStableId
      // before dropping m_protoStableIds: mints are globally unique, so the
      // next renderer misses the cache and allocates fresh slabs while these
      // sit orphaned until sweepMeshSlabs ages them out. Routed through the
      // grace queue so an in-flight command buffer stays safe.
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
    m_cachedDynamicSlotFingerprint = 0;
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

    // Persist-across-rebuild contract: when the OutputNode-owned registry is
    // the same pointer as in the previous init()/release() cycle, every slot
    // index and texture-array channel is still alive, so skip the cache wipe
    // -- re-allocating on a viewport resize or relink would re-upload the
    // decoded textures. The fingerprint / per-draw / cascade caches then match
    // the unchanged scene state on the first frame and short-circuit the
    // rebuild.
    //
    // m_registry is null between release() and init(), so a stray
    // rebuildChannel hits its guarded early-out; m_lastRegistry holds the
    // pre-release pointer. Null means first ever init; different means the
    // OutputNode built a fresh registry and every slot index is stale.
    auto* new_registry = &renderer.registry();
    const bool registry_changed = (m_lastRegistry != new_registry);
    if(registry_changed)
    {
      // Drop every per-registry cache before swapping m_registry: state left
      // by a previous RenderList (incremental edge rebuild with no
      // intervening release) holds slot indices the new registry never
      // allocated. freeRegistryResources=false because the old registry may
      // already be torn down -- forget the bookkeeping, do not free against it.
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
    // consumers building samplers in their own init() get a real texture
    // pointer from textureForOutput. update() reallocates with the real layer
    // count once the scene is flattened. Registry state is shared, so only the
    // first preprocessor to reach init() does this.
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
    // QRhiBuffer invariant: release through RenderList::releaseBuffer, which
    // skips deleteLater for a buffer still referenced by a mesh's MeshBuffers
    // -- RenderList::release destroys those itself.
    auto dropBuf = [&](QRhiBuffer*& b) {
      if(b) { renderer.releaseBuffer(b); b = nullptr; }
    };
    dropBuf(m_lightIndicesBuffer);
    // scene_materials and scene_lights bind the registry arenas directly.
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
    dropBuf(m_instAttribs);
    dropBuf(m_instDrawIds);
    m_instAttribsCap = 0;
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
    // Symmetric clear for m_pendingGpuCopies: its ops hold raw QRhiBuffer* for
    // buffers dropBuf just released.
    m_pendingGpuCopies.clear();
    m_pendingGpuCopies.shrink_to_fit();
    m_lastGpuCopiesFrame = -1;
    // Env arena buffer is owned by GpuResourceRegistry — nothing to drop here.

    // Free per-registry resources on every release(), whether the renderer is
    // about to be destroyed (recreateOutputRenderList) or reused
    // (relinkGraph). Skipping the free on a registry-pointer match would only
    // help relinkGraph, and would leak m_envSlot -- the Env arena has 8 slots,
    // so a handful of resizes exhausts it and the env binding falls back to
    // stale data. relinkGraph pays a re-allocation instead; it is rare, while
    // drag-resize fires continuously.
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

  // Mesh-deterministic subset of emitDraw's skip predicate: a draw is dropped
  // when the mesh has no usable positions, or has indices that are GPU-backed.
  // Both depend only on the mesh's buffers, which are invariant while the mesh
  // fingerprint matches, so the fast path can replicate them to keep its
  // freshPerDraws mirror in lock-step with what emitDraw packed. The other
  // emitDraw skips are handled at the call site or cannot occur once a slab is
  // resident.
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

  // Grow-only allocate / reuse a single QRhiBuffer, releasing the old handle
  // through RenderList::releaseBuffer (see the dropBuf note in release()).
  //
  // Returns true when the buffer was (re)allocated. A caller pairing the
  // buffer with a diffUpload-managed CPU mirror MUST clear that mirror on
  // true: diffUpload's equal-prefix short-circuit would otherwise leave the
  // prefix of the new, uninitialised allocation unwritten whenever the fresh
  // values happen to match the previous frame's.
  static bool growBuf(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      QRhiBuffer*& buf, int64_t& cap,
      int64_t need, QRhiBuffer::UsageFlags flags, const char* name)
  {
    if(buf && cap >= need)
      return false;
    // Power-of-two doubling overshoots for large buffers: a 1.08 GB request
    // lands on 2 GB, which QRhi backends commonly reject (maxStorageBufferRange
    // caps at 2GB-4, or the backend uses a signed 32-bit size). Below a 256 MB
    // knee, double; above it, grow 25 % over need. Aligned to 16 B so std430
    // structures land on natural strides.
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
    // create() returns false on driver-level allocation failure: out of VRAM,
    // over maxBufferSize, signed-32-bit overflow in the backend. A zombie
    // wrapper would turn uploadStaticBuffer into a silent no-op and every
    // shader read into zeroes, so surface it loudly.
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
      // Vulkan does not zero-initialise a new VkBuffer. Sparse-uploaded SSBOs
      // (per_draws past drawCount, unused world_transforms slots) would
      // otherwise feed whatever the device-memory page last held into the
      // shaders. RhiClearBuffer takes its zero bytes from a thread-local pool.
      RhiClearBuffer::clearBuffer(
          *renderer.state.rhi, res, buf, 0, (quint32)newCap);
    }
    cap = newCap;
    return true;
  }

  // Resolve a material_component to its Material-arena slot index.
  // Producer-authored materials carry a live raw_slot; loader materials get one
  // in m_loaderMaterialSlots. Returns 0 when none is found, which is an unused
  // arena entry, so shaders read a default MaterialGPU rather than garbage.
  //
  // This is the value stamped into PerDrawGPU.material_index, not the
  // scene.state->materials index. Both the fast-path pack and rebuildMDI must
  // go through here so the two agree.
  uint32_t arenaSlotForMaterial(const ossia::material_component* mat) const noexcept
  {
    if(!mat || !m_registry)
      return 0u;
    // isLiveIn, not isLive: this index is stamped into
    // PerDrawGPU.material_index and the shader reads scene_materials,
    // scene_materials_ext and scene_material_uv_xforms with it. isLive()
    // validates against whichever arena the ref names, so a raw_slot crossed
    // in from another arena -- RawTransform has 16384 slots against the ext
    // buffers' handful -- would pass and index far outside them. A36 showed
    // that class of read is an MMU fault, not merely wrong pixels.
    if(m_registry->isLiveIn(mat->raw_slot, GpuResourceRegistry::Arena::Material))
      return mat->raw_slot.internal_index;
    auto it = m_loaderMaterialSlots.find(mat);
    if(it != m_loaderMaterialSlots.end() && it->second.valid())
      return it->second.slot_index;
    return 0u;
  }

  // Resolve a stable id for an instance prototype. Producers should stamp
  // mesh_primitive::stable_id at construction; when one does not -- notably
  // Threedim::Primitive, routed through halp::geometry ->
  // mesh_component::legacy_geometry, which carries no primitive list -- mint an
  // id keyed on the mesh_component pointer, stable while the producer re-emits
  // the same shared_ptr.
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
  // vertex / index buffers and emit one output geometry with indirect draw
  // metadata. GPU-backed or non-standard-format draws are skipped with a
  // warning; they can still be rendered through per-mesh mode.
  //
  // TODO: replace the concatenated uploadStaticBuffer at offset 0 with
  // per-slab registry.uploadMeshStream calls gated on slab->freshly_allocated,
  // so adding one mesh uploads only that mesh's bytes.
  //
  // Primitive-cloud branch: buckets fs.primitive_clouds by format_id and emits
  // one indirect-draw geometry per bucket, appended to m_outputSpec.meshes
  // after the mesh MDI entry. Per bucket: `raw_splats`, `cloud_meta` and
  // `cloud_id_lookup` auxiliary SSBOs plus one indirect cmd
  // {vertex_count=6, instance_count=Σ primitive_counts}. The format's first CSF
  // stage reads raw_splats through AUXILIARY LAYOUT, keeping the descriptor
  // budget tight on integrated Metal.
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

      // Upfront sizing, also used by the per-bucket geometry construction
      // below. raw_splats needs VertexBuffer alongside StorageBuffer: the
      // bucket exposes it both as an AUXILIARY SSBO and as a per-vertex
      // ATTRIBUTE, and Vulkan requires VK_BUFFER_USAGE_VERTEX_BUFFER_BIT for
      // vertex bindings.
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

      // Delta-update fingerprint over everything the four GPU buffers depend
      // on. A match means the buckets are byte-equal to last frame's upload,
      // so the CPU concat and the four uploadStaticBuffer calls are skipped.
      // In the steady state the whole bucket loop is O(draws.size()) hashing.
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

      // 0 forces the first frame's upload. growBuf may also have just
      // allocated a fresh VkBuffer, but then total_primitives or row_stride
      // changed, which the fingerprint covers, so the re-upload branch runs.
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
          // loop below, but kept per-cloud so format CSFs can
          // frustum-cull individual clouds inside a bucket.
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

        // One cmd, vertex_count = one slot per primitive. The bucket geometry
        // is a flat point cloud; the downstream CSF stage reads
        // $VERTEX_COUNT_geoIn = N and emits the instanced 6xN quad topology.
        // A format chain may rewrite this cmd post-cull; the unculled total is
        // the safe default.
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

      // Expose the cloud->primitive mapping as a per-vertex ATTRIBUTE, not as
      // AUXILIARY: the CSF binder turns ATTRIBUTES into named SSBOs reachable
      // as geo_cloud_id_in[idx], and a read_only ATTRIBUTE on the input
      // geometry is what makes the CSF node create an input port at all.
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

      // When the producer named a struct type for the per-row payload, expose
      // raw_splats also as a user_struct ATTRIBUTE: the CSF binder then emits a
      // `Splat3DGS geo_splat_in[]` declaration matching the consumer's TYPES
      // block, so shaders read rows as ISF_READ(geoIn, splat)[idx].field. The
      // AUXILIARY entry above stays for presets that predate TYPES.
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
            .byte_size = cameraAuxByteSize(m_cachedCameras.size())});
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

      // Surface format_id as filter_tag (rapidhash truncated to 32 bits) so a
      // downstream FlattenedSceneFilterNode in "format_id == match_str" mode
      // routes this bucket to its format-specific chain. Same hash as the
      // bucket key, so bucketing and filtering agree. Empty format_id leaves
      // filter_tag at the 0 "untagged" sentinel.
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
    // Per-mesh slab allocation. Per draw: acquireMeshSlab(stable_id, vc, ic)
    // against the 5 per-stream OffsetAllocators in GpuResourceRegistry;
    // on freshly_allocated, extract CPU bytes (or queue a GPU copy for
    // GPU-backed sources) and uploadMeshStream into the slab's byte offset;
    // indirect_draw_cmds baseVertex / firstIndex come from those offsets
    // divided by stream stride; markMeshSlabSeen keeps the per-frame sweep off
    // it. The grace queue holds a reclaimed offset for 2 frames so an in-flight
    // draw cannot see it reused.
    //
    // Output layout is four vertex bindings (pos/nrm/uv/tan), one index buffer
    // and the scene auxiliaries. Vertex/index buffers are registry-owned and
    // pre-sized at registry init, so nothing here grows or bulk-uploads them;
    // this function owns only the per_draws + indirect_draw_cmds upload, the
    // per-draw metadata pack and the output geometry construction.
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

    // Concat offsets for joint matrices across every skeleton in this flatten:
    // skinJointOffsets[k] = sum of joint counts for skins < k. Stamped into
    // PerDrawGPU.skeleton_offset, with 0xFFFFFFFF for unskinned draws, so a
    // future consolidated joint_matrices SSBO needs no shader change.
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

    // Running cursor into the unified per-instance concat space. Each emitted
    // cmd consumes instanceCount contiguous slots and writes its own cmd-index
    // into draw_ids[slot..]. For instance groups cmd_index != slot index, so
    // the shader reads the per-instance draw_id attribute this cursor
    // populates rather than gl_BaseInstance or gl_DrawID.
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

    // Shared per-cmd processor for the fs.draws and fs.instances loops:
    // attribute extraction, slab acquire and per-stream upload, per_draws and
    // per_draw_bounds push, indirect cmd push with firstInstance = slot_cursor,
    // then slot_cursor += instanceCount. Returns the emitted cmd_index, or
    // kCmdSkipped.
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

    // fs.instances: one cmd per instance_component, instanceCount = the group's
    // instance count, firstInstance = slot_cursor. Per-instance translations
    // and colors are GPU-copied from the upstream Instancer's buffers at
    // slot_base * stride; draw_ids[slot..] are filled after both loops, once
    // slot_cursor has stopped moving.
    //
    // A group whose buffer handles are still null is skipped for the frame:
    // the upstream Instancer may be mid-rebuild and will be ready next frame.
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

    // drawCount == 0: procedural-only consumers (skybox, fullscreen-triangle
    // effects) still need the scene-wide aux table, and `camera` rides on the
    // geometry, so build a 0-vertex carrier mesh exposing the full auxiliary
    // list. Mesh-consuming nodes see vertices == 0 and skip their draw. The
    // uploads below are gated on non-empty sources; binding extents fall back
    // to one element so RHI accepts them.

    const int64_t pdBytes = std::max<int64_t>(
        sizeof(PerDrawGPU),
        (int64_t)acc.perDraws.size() * sizeof(PerDrawGPU));
    const int64_t icBytes = std::max<int64_t>(
        sizeof(Acc::IndirectCmd),
        (int64_t)acc.indirectCmds.size() * sizeof(Acc::IndirectCmd));
    const int64_t pdbBytes
        = (int64_t)acc.perDrawBounds.size() * sizeof(PerDrawBoundsGPU);

    // Grow-only for the preprocessor-owned small SSBOs; arena streams are
    // pre-sized in registry.init() and never grow. On realloc, drop the
    // diff-upload mirror so the next diffUpload treats the new buffer as empty
    // -- see growBuf's prefix-staleness note.
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
    // Two arrays sized to slot_cursor: draw_ids[k] is the cmd index owning
    // slot k; attribs[k] is a 32-byte slot holding the vec4 translation then
    // the vec4 color, identity for regular cmd slots and GPU-copied
    // per-particle values for instance-group slots.
    //
    // Layout invariant: a regular fs.draws cmd at acc index i lands at slot i
    // (instanceCount == 1); instance groups follow contiguously, with the
    // per-group bookkeeping in instanceRecords. The shader reads draw_id as a
    // per-instance VERTEX_INPUT and indexes per_draws[draw_id], which works on
    // both the indirect and the CPU-fallback path because firstInstance is the
    // only state involved.
    if(slot_cursor > 0)
    {
      const int64_t drawIdsBytes = (int64_t)slot_cursor * 4;
      const int64_t attribsBytes = (int64_t)slot_cursor * kInstSlotStride;

      // m_instDrawIds is diff-uploaded against m_cachedInstDrawIds, so the
      // mirror MUST be cleared on realloc: an Instancer with one prototype
      // writes the same draw_id into every slot, cached and fresh match for the
      // whole prefix, and diffUpload's equal-runs branch would leave the new
      // buffer's prefix as uninitialised driver memory. Translations and colors
      // are fully GPU-copied and do not need it.
      if(growBuf(renderer, res,m_instDrawIds, m_instDrawIdsCap, drawIdsBytes,
                 UF(QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer),
                 "ScenePreprocessor::inst.draw_ids"))
        m_cachedInstDrawIds.clear();
      growBuf(renderer, res,m_instAttribs, m_instAttribsCap, attribsBytes,
              UF(QRhiBuffer::VertexBuffer | QRhiBuffer::StorageBuffer),
              "ScenePreprocessor::inst.attribs");

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

      // Regular-slot identity values for translations + colors, written as
      // whole interleaved slots: zero translation then white color. Instance
      // group slots (offset >= n_regular_cmds * kInstSlotStride) are filled by
      // the GPU copies below — uploadStaticBuffer here covers ONLY the
      // regular range so we don't stomp the GPU-copied data. Instance
      // group slot ranges that overlap stale content from a previous
      // frame are overwritten by the per-frame GPU copy.
      //
      // The growBuf above is the one write in this batch that DOES overlap
      // the GPU-copied ranges: on a (re)allocation it zero-clears the whole
      // new capacity, which is necessary (Vulkan hands back uninitialised
      // device memory) and cannot be narrowed here, since only the copies
      // that follow know which slots they will cover. That overlap is a
      // transfer-after-transfer hazard, resolved by the source scope of
      // issuePendingGpuCopies' batch barrier, not by dropping a write.
      if(n_regular_cmds > 0)
      {
        std::vector<float> regular_slots(n_regular_cmds * 8, 0.f);
        for(std::size_t i = 0; i < n_regular_cmds; ++i)
          for(int c = 4; c < 8; ++c)
            regular_slots[i * 8 + c] = 1.f;
        res.uploadStaticBuffer(
            m_instAttribs, 0,
            (quint32)(n_regular_cmds * kInstSlotStride),
            regular_slots.data());
      }

      // Queue GPU copies for instance groups: `count` instances from the
      // upstream Instancer's source buffer into the concat array at
      // slot_base * stride. src_translation_offset is biased per source format
      // so each strided slice lands on the translation bytes:
      //  - translation (vec4): bytes [0..15].
      //  - trs (vec3 T + ...): bytes [0..15]; the shader binds vec3 from
      //    offset 0, so the stray R bytes are never sampled.
      //  - mat4 (col-major):   bytes [48..63], column 3.
      //
      // The destination slot is 32 bytes wide and the element is 16, so both
      // of these are strided writes even when the source is tight: every copy
      // lands on its half of the interleaved slot and must step over the
      // other half.
      auto queueInstanceCopy = [&](
          QRhiBuffer* src, uint32_t srcOffset, uint32_t srcStride,
          QRhiBuffer* dst, uint32_t dstOffset, uint32_t dstStride,
          uint32_t count, uint32_t elemSize)
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
        op.dst_stride = (int)dstStride;
        op.element_size = (int)elemSize;
        const bool tight
            = (op.src_stride == 0 || op.src_stride == op.element_size)
              && (op.dst_stride == 0 || op.dst_stride == op.element_size);
        op.size = tight ? op.vertex_count * op.element_size : op.element_size;
        m_pendingGpuCopies.push_back(op);
      };
      for(const auto& rec : instanceRecords)
      {
        // Translation: copy 12 bytes per instance into the leading
        // bytes of each slot's translation half. The half's trailing 4 bytes
        // remain garbage / leftover (identity uploads only cover the
        // regular range above) — the shader binds vec3 from offset 0
        // so the trailing pad is never sampled.
        if(rec.src_translations)
        {
          queueInstanceCopy(
              rec.src_translations, rec.src_translation_offset,
              rec.src_translation_stride,
              m_instAttribs,
              rec.slot_base * kInstSlotStride + kInstTranslationOffset,
              kInstSlotStride, rec.count,
              /*elemSize=*/16);
        }
        if(rec.src_colors)
        {
          queueInstanceCopy(
              rec.src_colors, rec.src_color_offset, /*srcStride=*/16,
              m_instAttribs,
              rec.slot_base * kInstSlotStride + kInstColorOffset,
              kInstSlotStride, rec.count,
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

    // MDI uses a 16-byte stride for position and normal even though the shader
    // binding format is float3: GPU-resident vertex sources emit vec3 inside a
    // 16-byte-aligned slot under std430/std140, so matching the stride turns a
    // per-vertex strided copy loop into a single blit. Costs 33 % extra memory
    // on pos/nrm only.
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
                        decltype(ossia::geometry::attribute::format) fmt,
                        int byte_offset = 0) {
      ossia::geometry::attribute a{};
      a.binding = binding;
      a.byte_offset = byte_offset;
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
    // Two PerInstance step_rate=1 bindings. Each indirect cmd sets
    // firstInstance to its own slot offset, so they address the right slice of
    // each concat buffer on both the indirect and the CPU-fallback path.
    //
    // Translation and color share the first of them: they are both
    // per-instance vec4s stepping at the same rate, and the eight bindings
    // above plus three of their own would be nine — one more than
    // QD3D11CommandBuffer::MAX_VERTEX_BUFFER_BINDING_COUNT, which Qt's D3D11
    // backend silently clamps to (qrhid3d11.cpp, "Too many vertex buffer
    // bindings"). Interleaved they are eight, and a shader reading every
    // stream this geometry publishes fits.
    //
    // Buffer slot order in g.buffers: 0..5 per-vertex streams
    // (pos/nrm/uv0/tan/col/uv1), 6 index, 7 inst_attribs, 8 inst_draw_ids.
    // Inserting a slot here shifts every subsequent aux index; the auxiliary
    // section computes its own base from g.buffers.size().
    if(slot_cursor > 0 && m_instAttribs && m_instDrawIds)
    {
      // Index buffer must come before per-instance buffers since
      // g.index.buffer is hard-coded to slot 6 below; per-instance
      // buffers occupy slots 7 and 8.
      g.buffers.push_back(wrapGpu(
          m_instAttribs, (int64_t)slot_cursor * kInstSlotStride));
      g.buffers.push_back(wrapGpu(
          m_instDrawIds, (int64_t)slot_cursor * 4));

      ossia::geometry::binding bInst{};
      bInst.byte_stride = kInstSlotStride;
      bInst.classification = ossia::geometry::binding::per_instance;
      bInst.step_rate = 1;
      const int instBindIdx = (int)g.bindings.size();
      g.bindings.push_back(bInst);

      ossia::geometry::binding bInstD{};
      bInstD.byte_stride = 4;
      bInstD.classification = ossia::geometry::binding::per_instance;
      bInstD.step_rate = 1;
      const int instDBindIdx = (int)g.bindings.size();
      g.bindings.push_back(bInstD);

      g.input.push_back(GeomInput{.buffer = 7, .byte_offset = 0});
      g.input.push_back(GeomInput{.buffer = 8, .byte_offset = 0});

      // Translation reuses the `translation` semantic; no per-vertex
      // translation exists, so there is no collision. Color uses the
      // `instance_color0` semantic, added to libossia to avoid colliding with
      // per-vertex `color0` in findGeometryAttribute. draw_id uses the
      // uint-typed `instance_draw_id`.
      pushAttr(ossia::attribute_semantic::translation,
               instBindIdx, ossia::geometry::attribute::float3,
               kInstTranslationOffset);
      pushAttr(ossia::attribute_semantic::instance_color0,
               instBindIdx, ossia::geometry::attribute::float4,
               kInstColorOffset);
      pushAttr(ossia::attribute_semantic::instance_draw_id,
               instDBindIdx, ossia::geometry::attribute::uint1);
    }

    g.vertices  = (int)m_mdi.totalVertices;
    g.indices   = (int)m_mdi.totalIndices;
    g.instances = 1;
    g.topology  = ossia::geometry::triangles;
    // glTF doubleSided: pipeline culling is off for the MDI batch and culling
    // is per-fragment, driven by each material's feature_mask. Single-sided
    // materials discard !gl_FrontFacing fragments (equivalent to CULL_BACK);
    // double-sided ones keep both and flip the normal on back faces. Splitting
    // the batch by cull mode would multiply the draw count and lose most of the
    // indirect-draw benefit.
    g.cull_mode = ossia::geometry::none;
    g.front_face = ossia::geometry::counter_clockwise;

    g.index.buffer = 6;  // Slot order: pos=0, nrm=1, uv=2, tan=3, col=4, uv1=5, idx=6.
    g.index.byte_offset = 0;
    g.index.format = decltype(g.index)::uint32;

    // filter_tag / filter_material_index are per-geometry metadata for
    // mesh-level filters. One geometry spans many materials here, so no single
    // value is meaningful: stamp 0 and let those filters keep or drop the whole
    // batch. Per-draw filtering belongs to a CSF filter consuming
    // indirect_draw_cmds + per_draws.
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
    // stride matches sizeof(MaterialGPU) = 80B.
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
    // Env UBO: bind the preprocessor-owned slot. merge_scenes composes the
    // merged scene_environment field by field from every contributing loader,
    // so no single producer's slot holds the result.
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
    // Parallel to scene_materials: same element count, same indexing.
    // OpenPBR-grade shaders bind this as a second SSBO and read it with the
    // same material_index. byte_size is the full buffer capacity, sized in
    // update() to (max_arena_slot + 1) * sizeof(MaterialExtensionsGPU), because
    // the shader indexes by arena slot.
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
        .byte_offset = 0,
        .byte_size = cameraAuxByteSize(m_cachedCameras.size())});
    g.auxiliary.push_back({
        .name = "camera_prev", .buffer = baseBuf + 7,
        .byte_offset = 0,
        .byte_size = cameraAuxByteSize(m_cachedCameras.size())});
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

    // shadow_cascades UBO, 544 B std140, populated from
    // scene_state.shadow_cascades. Always published: cascade_count == 0 tells
    // consumers to skip shadow sampling.
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

    // Mid-pipeline aux injection from upstream InjectBuffer / InjectTexture.
    // Appended after the preprocessor's own entries; colliding earlier entries
    // are pre-removed below so find_auxiliary resolves last-wins.
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

    // Reuse the indirect_count slot for the draw count; renderers supporting
    // drawIndexedIndirect pick it up automatically.
    //
    // On the drawCount == 0 carrier path, leave indirect_count.handle null so
    // CustomMesh::drawSingleMesh skips its indirect branch -- it would
    // otherwise draw from a buffer nothing uploaded this frame. The carrier is
    // still published for procedural-only consumers, which read the auxiliary
    // list; mesh consumers fall through to a no-op cb.draw(0, 0).
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


  // Decode a texture_source to an RGBA8888 QImage. Single decode point, so the
  // rebuild below can dedupe upstream of JPEG decoding. With a non-zero
  // content_hash and an AssetTable available, peek the cache first and stage on
  // miss so other RenderLists and reloads within the session hit it. Zero-hash
  // sources always decode.
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

  // Fingerprint of the registry's dynamic texture-slot table: for every
  // material-texture channel, the ordered (slot -> QRhiTexture identity)
  // mapping resolveDynamicSlot maintains.
  //
  // That table is the ONLY thing a live texture-handle reroute changes.
  // computeMaterialsFingerprint below is keyed on material stable_id, which is
  // deliberately preserved across such a swap (a MaterialOverride clone
  // inherits it), so sameMaterialsContent stays true and rebuildChannel takes
  // its fast path: no bucket array is reallocated, hence no channelReallocated
  // and no auxBuffersChanged. Yet the table drives two pieces of state the
  // consumer must re-read:
  //   - the "<channel>Dyn<slot>" auxiliary_textures appendTextureAuxes emits
  //     into the geometry, which name dynamicTextures[slot] by pointer and are
  //     only refreshed when rebuildMDI republishes m_outputSpec.meshes;
  //   - MaterialGPU::textureRefs[ch] = tex_ref_dynamic(slot), which only
  //     reaches the arena through the gated updateSlot pass below.
  // Both therefore need a term of their own — see the two use sites in
  // update(). Cost is bounded by ChannelCount * kMaxDynamicSlots (5 * 4), so
  // ~25 hash_combine per frame regardless of scene size.
  //
  // Keyed on globalResourceId, not the raw pointer, for the same
  // pointer-recycling reason resolveDynamicSlot keys its map on it: a freed
  // QRhiTexture's address can be handed back to a fresh one, which would make
  // a real reroute look like no change at all.
  uint64_t computeDynamicSlotFingerprint() noexcept
  {
    uint64_t fp = 0;
    if(!m_registry)
      return fp;
    for(int i = 0; i < ChannelCount; ++i)
    {
      const auto& dyn
          = texChannel(static_cast<MaterialChannel>(i)).dynamicTextures;
      ossia::hash_combine(fp, (uint64_t)dyn.size());
      for(auto* t : dyn)
        ossia::hash_combine(fp, t ? (uint64_t)t->globalResourceId() : 0ull);
    }
    return fp;
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
  // texture_source pointer so N materials sharing one image upload one layer.
  // Patches fs.materials[i].textureRefs[ch] with the packed layer ref.
  // `sameMaterialsContent` is the once-per-update() comparison of `fp` against
  // m_cachedMaterialsFingerprint, passed in so the per-channel calls do not
  // each re-walk the list. Returns true if the QRhiTexture* was reallocated,
  // which the caller uses to trigger downstream SRB rebinds.
  //
  // rebuildDynamicSlots assigns slot indices for texture_refs carrying a GPU
  // handle without a source. Rebuilt every frame: the upstream QRhiTexture*
  // can swap without the material_component pointer changing. O(n_mats), no
  // uploads; materials past the slot cap recycle the LRU-oldest slot.
  void rebuildDynamicSlots(MaterialChannel ch)
  {
    // Dynamic slot maps live for the registry's lifetime, cleared only in its
    // init()/destroy(). resolveDynamicSlot is idempotent on the same
    // QRhiTexture*, so this pass is a no-op for unchanged handles and only
    // refreshes the LRU stamp; producers calling it earlier agree on the same
    // slot index.
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

    // Multi-bucket texture arrays: each distinct (format, size, sampler_config)
    // tuple gets its own bucket, and materials reference
    // tex_ref_static(bucket_id, layer_id).
    //
    // Per rebuild: clear the layerMaps, walk materials decoding each unique
    // source once and routing it to findOrCreateBucket, reallocate the
    // QRhiTextureArray of every bucket whose size or layer count changed,
    // upload into the assigned (bucket, layer) slots, and keep at least one
    // fallback layer in bucket 0 so the unsuffixed binding stays valid.
    //
    // Every bucket is RGBA8 today; HDR and compressed formats plug in by
    // varying the format argument.

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
      // Process one static texture_ref into this channel's bucket pool. Used
      // for the main channel ref and for every ext-table ref whose channel
      // matches, so new ext slots inherit dedup, decode-fail handling and
      // bucket-cap diagnostics.
      //
      // is_main_occlusion enables the glTF MR-r packed-occlusion shortcut,
      // which applies only to the main occlusion ref; the comparison needs the
      // material's MR source, passed as mr_source_for_occ_check.
      const auto register_static_ref
          = [&](const ossia::texture_ref& tref,
                const ossia::texture_source* mr_source_for_occ_check,
                bool is_main_occlusion) {
        const auto* s = tref.source.get();
        if(!s)
          return;

        // Occlusion-from-MR shortcut: when the occlusion and
        // metallic-roughness textures share a source, the shader reads
        // occlusion from MR.r * factor per the glTF packing convention, so no
        // separate occlusion layer is needed. patchMaterialRefsFromCache emits
        // tex_ref_none() for the occlusion ref and the feature_mask bit stays
        // clear.
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

        // Route to a bucket keyed on (format, size, sampler_config). Splitting
        // on sampler_config honours per-texture wrap/filter modes when several
        // materials share a channel array; most glTFs use a single sampler, so
        // it collapses to one bucket per (format, size).
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
        // Material textures are uploaded with a full mip chain
        // (TextureLoader.cpp uploadImageToTexture), so promote the bucket
        // sampler to trilinear: mag/min filter NONE becomes LINEAR, mipmap_mode
        // NONE becomes LINEAR. NEAREST is preserved -- that is an explicit
        // author choice for pixel-art assets. glTFs commonly declare
        // minFilter=LINEAR rather than LINEAR_MIPMAP_LINEAR, which would
        // otherwise sample mip 0 only.
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
    // layer count, and how many sources got dropped.
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

    // Encode a texture_ref into a packed uint per the tex_ref_static /
    // tex_ref_dynamic / tex_ref_none scheme. Dynamic handles are looked up
    // first, since a GPU handle takes precedence over a CPU source when both
    // are set; static sources are matched against the per-bucket layerMap.
    // Returns tex_ref_none() for empty refs, dynamic-slot-cap overflow, and
    // static sources that failed to map.
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

      // One `auxiliary_texture` per live bucket, named
      // <channelName><bucket_id>, capped at kMaxBuckets. Consumer shaders
      // declare matching sampler2DArray INPUTS and switch on the 6-bit bucket
      // field of MaterialGPU::textureRefs.
      //
      // Bucket 0 is also emitted under the unsuffixed <channelName> for shaders
      // that only decode bucket 0. Such a shader shown a multi-bucket scene
      // renders bucket 0's layer in place of the intended one; those presets
      // should migrate to a ladder-aware one. No overhead for the common
      // single-bucket case.
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
      // Scene-wide environment textures published under well-known aux names on
      // the existing scene cable; consumer shaders declare matching INPUTS and
      // the aux resolver picks them up.
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

  // Pack every camera collected by flattenScene into a std140 UBO array. Slot 0
  // is the active camera, the rest follow in insertion order; a scene with no
  // cameras gets one synthesized default so the binding is always valid.
  // Diff-uploaded against m_cachedCameras.
  void packAndUploadCameras(
      RenderList& renderer, QRhiResourceUpdateBatch& res, const FlatScene& fs)
  {
    // Once per frame, not once per outgoing edge: the snapshot-before-overwrite
    // below seeds camera_prev from m_cachedCameras and then replaces it, so a
    // second call in the same frame would make camera_prev == camera and zero
    // out motion. RenderList::frame is a reliable per-frame token.
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

    // Upload `camera_prev` from the CPU mirror of what is currently in the GPU
    // `camera` buffer, i.e. last frame's content, since `fresh` overwrites it
    // below. On the first frame m_cachedCameras is empty, so seed prev with
    // current and motion comes out zero. Same snapshot-before-overwrite pattern
    // as m_worldTransformsPrevBuffer: prev is a function of the GPU buffer's
    // last frame, not of cache-hit history. Current is uploaded unconditionally
    // -- the diff-skip saved under 4 KB of Dynamic-UBO churn per frame.
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
    // Re-flatten on content change, not on "a push happened this frame":
    // producers re-push every frame so multi-source scenes stay consistent, and
    // NodeRenderer's merge cache keeps the scene_state shared_ptr stable when
    // no input changed, which makes pointer + version a reliable content test.
    bool needsRebuild = !m_outputSpec.meshes;
    if(this->scene.state.get() != m_cachedSceneState)
      needsRebuild = true;
    if(this->scene.state && this->scene.state->version != m_cachedVersion)
      needsRebuild = true;

    // Always refresh the camera UBOs every frame, regardless of whether
    // mesh-rebuild fires: motion-vector reprojection needs a per-frame
    // camera_prev refresh, which the rebuild gate cannot guarantee.
    // packAndUploadCameras synthesises a default camera when fs.cameras
    // is empty, so this runs unconditionally — keeps m_camerasBuffer
    // allocated and bound even when no scene producer is wired yet.
    //
    // Gate the whole camera flatten+upload on a per-frame token: update()
    // is dispatched once per outgoing edge, and the flattenScene() feeding
    // it is NOT free, so it must run at most once per frame regardless of
    // edge count.
    if(m_lastCameraUploadFrame != renderer.frame)
    {
      FlatScene cameraFs;
      flattenScene(this->scene, cameraFs, /*aspectRatio=*/1.f);
      packAndUploadCameras(renderer, res, cameraFs);
    }

    if(!needsRebuild)
    {
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

      // Build / refresh every material-texture channel and patch
      // fs.materials[i].textureRefs[ch] with the assigned layer indices, before
      // the scene_materials SSBO upload below.
      //
      // Each channel owns a QRhiTextureArray (sRGB for base color and emissive,
      // linear for MR and normal, see channelFlags). A realloc changes the
      // emitted auxiliary_texture's native_handle, which downstream only picks
      // up when geometryChanged fires, i.e. on a fresh meshes shared_ptr -- so
      // the realloc rolls into the same auxBuffersChanged flag the SSBO-grow
      // path uses, and rebuildMDI() rebuilds the meshes vector.
      //
      // The materials list is fingerprinted once and the equality result passed
      // to each channel.
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

      // The rebuildChannel loop above ran rebuildDynamicSlots for every
      // channel, so the registry's dynamic-slot table is now final for this
      // frame (sweepStaleDynamicTextureSlots only runs from rebuildMDI, on the
      // full-rebuild branch, and is accounted for when the cache is reseeded
      // there). Snapshot it: a reroute changes nothing else this node looks at.
      const uint64_t freshDynamicSlotFingerprint
          = computeDynamicSlotFingerprint();
      const bool dynamicSlotsChanged
          = (freshDynamicSlotFingerprint != m_cachedDynamicSlotFingerprint);

      // Loader-material arena slot upload: now that rebuildChannel has
      // patched fs.materials[i].textureRefs with the resolved per-channel
      // layer indices, stream each loader material's packed MaterialGPU
      // bytes into its Material arena slot. Producer-authored materials
      // (PBRMesh, MaterialOverride-if-migrated, CSF mesh producers) keep
      // their own slot fresh in their update() hooks — we skip those.
      //
      // Uploads happen only when the materials content actually changed
      // (sameMaterialsContent==false) OR when a channel reallocated and
      // shifted layer indices OR when the dynamic-slot table moved, which
      // renumbers tex_ref_dynamic(slot) without touching either of the other
      // two. Steady-state frames with an unchanged scene touch zero bytes
      // here.
      if(m_registry && this->scene.state
         && (!sameMaterialsContent || channelReallocated
             || dynamicSlotsChanged))
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

      // Ensure the scene-wide SSBOs are large enough; a no-op unless the count
      // grew past the cap.
      //
      // scene_materials_ext and scene_material_uv_xforms are indexed in the
      // shader by Material ARENA SLOT (entries[pd.material_index], parallel to
      // scene_materials, which is the arena), so their CPU side must be sized
      // and filled by arena slot too, not by fs.materials position.
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
      // Instancer / loader prototype materials are not in
      // scene.state->materials but do hold an arena slot, and that slot is what
      // arenaSlotForMaterial -- hence PerDrawGPU.material_index -- resolves to
      // for their draws. Fold those slots into the extent so the aux buffers
      // cover every reachable material_index.
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
      // Returns true on (re)allocation; same prefix-staleness invariant as the
      // static growBuf above. Also zero-fills the new buffer, since Vulkan does
      // not zero a VkBuffer on creation and these SSBOs are sparsely uploaded.
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
      // Realloc → clear the diffUpload mirror so the freshly-allocated
      // GPU buffer's prefix isn't left as garbage.
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
      // 16 KiB floor = 4096 light index slots, so override CSFs can publish up
      // to 4k procedural lights without clamping to the scene-graph-derived
      // size. Must stay equal to Arena::RawLight's slot count * 4 bytes: too
      // low and procedural CSFs clamp early, too high and scene_light_indices
      // names slots past the arena.
      const int64_t lightIdxBytes
          = std::max<int64_t>(16384, (int64_t)freshLightIndices.size() * 4);
      if(grow(m_lightIndicesBuffer, m_lightIndicesCap, lightIdxBytes,
              "ScenePreprocessor::light_indices"))
        m_cachedLightIndices.clear();

      // scene_counts: 16 bytes, allocated once, Static + StorageBuffer.
      //
      // SSBO only, no UBO half: QRhi forbids Dynamic + StorageBuffer, and
      // Static + UniformBuffer fails create() on D3D11 and GLES, which do not
      // support NonDynamicUniformBuffers. All bundled shaders therefore declare
      // scene_counts as a storage buffer -- rasterizers with
      // TYPE: "storage", ACCESS: "read_only"; nested AUXILIARY declarations,
      // where SSBO is the default kind, with just ACCESS: "read_only";
      // override-CSFs that write it with ACCESS: "read_write".
      //
      // scene_counts.light_count reads identically either way: std140 and
      // std430 agree on a 4-uint struct. A shader author may still declare
      // TYPE: "uniform", but then owns the backend-support question.
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

      // The camera QRhiBuffer is allocated at the top of update(), so by
      // the time rebuildMDI runs below, m_camerasBuffer is non-null and
      // ready to be attached as an aux on the emitted geometry.

      // Pack the MERGED scene_environment into our own Env arena slot.
      // merge_scenes composes every producer's contribution field by field via
      // the params_set bitmask, so this->scene.state->environment is the final
      // state. Producers still write their own slots for any consumer wanting
      // per-producer data, but the scene_environment binding points here.
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
          // Zero-fill both buffers. world_transforms is sparse: only slots
          // used by an actual scene_transform get written, and a fresh
          // RenderList hands out a VkBuffer holding device-memory garbage, so
          // any consumer indexing world_transforms.data[L.transform_slot] for
          // an unpopulated slot reads it. _prev matters too, since the
          // copyBuffer(current -> prev) on the first frame would propagate it.
          //
          // RhiClearBuffer's batch variant pulls from the thread-local zero
          // pool, so both 1 MiB clears reuse one backing vector.
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

      // Pack per-draw data: one struct copy plus one aabb copy per draw.
      // pd.material_index is the Material-arena slot from arenaSlotForMaterial,
      // the same helper rebuildMDI uses, so the encoding matches on both paths.
      // transform_slot, skeleton_offset and per_draw_bounds are packed in
      // lockstep so the sidecar stays in sync for downstream culling CSFs.
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

      // Mesh fingerprint: the sequence of DrawCall::stable_id, i.e. the
      // addresses of the source mesh_primitives backing each draw. Those are
      // invariant while the mesh_component shared_ptrs and their primitives
      // vectors are, so walking the same tree twice gives identical
      // fingerprints. (dc.mesh is a fresh primitiveToGeometry() wrapper and
      // differs every frame.)
      //
      // The upstream GPU-resident attribute buffer handles are mixed in too:
      // m_pendingGpuCopies holds raw QRhiBuffer* captured at rebuildMDI time
      // and re-issued every frame, so an upstream buffer rebuild behind an
      // unchanged mesh_primitive address must force a full rebuild rather than
      // let the queue copy from a freed buffer.
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

      // Cloud fingerprint: rebuildPrimitiveClouds only runs on the full-rebuild
      // branch, so any change to the cloud set must mismatch this. Hashes the
      // same fields the function's per-bucket fingerprint and bucket geometry
      // depend on -- raw_data identity and content version, primitive_count,
      // transform_slot, the world matrix, and the format_id-derived bucket key.
      // Count is mixed first so a pure add or remove is always detected.
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

      // Pack per-material UV transforms (KHR_texture_transform) and material
      // extensions. The shader reads both as entries[pd.material_index], where
      // material_index is the Material arena slot, so these must be
      // arena-slot-indexed rather than fs.materials-indexed -- a one-material
      // scene whose loader material lands at arena slot 1 would otherwise read
      // out of bounds.
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
            // The dynamic texture-slot table moved: the published geometry's
            // "<channel>Dyn<slot>" auxiliary_textures name the OLD
            // QRhiTexture*s, and downstream only re-resolves them on
            // geometryChanged, which is shared_ptr identity on
            // m_outputSpec.meshes (NodeRenderer.cpp). Only rebuildMDI
            // republishes that vector, so a reroute has to leave the fast
            // path on its own account. Before this term it reached the screen
            // only when some unrelated aux buffer happened to grow in the
            // same frame.
            && !dynamicSlotsChanged
            // Cloud set unchanged: rebuildPrimitiveClouds only
            // runs on the full-rebuild branch and re-appends its bucket
            // geometries onto the freshly rebuilt mesh list, so any cloud
            // add / remove / move / re-upload must drop us off the fast path.
            && (freshCloudFingerprint == m_cachedCloudFingerprint)
            // freshPerDraws / freshMeshFingerprint cover fs.draws only;
            // fs.instances cmds are processed exclusively inside rebuildMDI(),
            // so any instance group present forces the full rebuild.
            && fs.instances.empty();

      if(meshesUnchanged)
      {
        // Fast path: diff-upload only the small scene-level SSBOs. The big
        // vertex/index/indirect buffers are untouched and m_outputSpec.meshes
        // keeps the same shared_ptr, so downstream does not even flag
        // geometryChanged. scene_lights is the RawLight arena, kept fresh by
        // producers; only the compact indices list needs uploading.
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
        //
        // The dynamic-slot fingerprint is RE-read here rather than seeded from
        // freshDynamicSlotFingerprint: rebuildMDI runs sweepMeshSlabs, which
        // sweeps orphaned dynamic slots to null, so the table the aux entries
        // were just built from is the post-sweep one. Seeding the pre-sweep
        // value would report a phantom change on the next frame.
        m_cachedDynamicSlotFingerprint = computeDynamicSlotFingerprint();
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

      // scene_counts SSBO: the authoritative N per SSBO, so shaders do not use
      // .length(), which reports capacity and includes zeroed tail slots after
      // a shrink. Uploaded only when a count changed. light_count is the
      // arena-addressable subset and drives the shaders' index-buffer loop.
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

      // shadow_cascades UBO, straight struct copy from
      // scene_state.shadow_cascades: light_view_proj[8] column-major,
      // split_view_depths[9] compacted into cascade_split_distances[8],
      // cascade_count. Diff-uploaded, so frames without topology or camera
      // changes cost nothing. Published even with cascade_count == 0 so shaders
      // declaring it as INPUT have a valid binding.
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
        // Shaders sample cascade_split_distances[k] as the far-plane view-space
        // Z of cascade k. The CPU side stores count+1 boundaries in
        // split_view_depths[], where entry k is the NEAR plane of cascade k, so
        // slot k must take boundary k+1.
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

  // Issue every pending GPU->GPU copy queued during update(). Runs every frame
  // whether or not update() rebuilt the accumulator: upstream buffer contents
  // change every frame under CSF compute writes while the handles and MDI
  // offsets stay stable until draw topology changes, at which point the queue
  // is rebuilt.
  //
  // Stride-equal-to-element copies collapse to one copyBuffer; strided
  // vec4->vec3 copies fall back to one copyBuffer per vertex.
  void issuePendingGpuCopies(RenderList& renderer, QRhiCommandBuffer& cb)
  {
    if(m_pendingGpuCopies.empty())
      return;
    auto* rhi = renderer.state.rhi;
    if(!rhi)
      return;
    cb.beginExternal();
    // One {compute,transfer}→transfer barrier for the whole batch instead of
    // one per copy call — eliminates N−1 redundant pipeline stalls on Vulkan.
    // The transfer half of its source scope is what orders these copies after
    // the QRhiResourceUpdateBatch RenderList already submitted this frame:
    // that batch's uploadStaticBuffer calls (growBuf's zero-clear of a freshly
    // allocated buffer above all) are themselves vkCmdCopyBuffer, and nothing
    // else orders them against these. Batch-wide is enough — the ops within
    // one batch write disjoint destination ranges.
    score::gfx::beginBufferCopyBarrier(*rhi, cb);
    // Scratch reused across ops — avoids reallocating for each strided op.
    std::vector<score::gfx::BufferCopyRegion> regions;
    for(const auto& op : m_pendingGpuCopies)
    {
      // Explicit dst wins over the mesh-stream lookup — used by the
      // unified-MDI per-instance concat copies (the interleaved attribs
      // array) which target preprocessor-owned buffers, not arena streams.
      QRhiBuffer* dst = op.dst ? op.dst : mdiBufferFor(op.attr);
      if(!op.src || !dst)
        continue;
      const int src_stride
          = op.src_stride == 0 ? op.element_size : op.src_stride;
      const int dst_stride
          = op.dst_stride == 0 ? op.element_size : op.dst_stride;
      if(src_stride == op.element_size && dst_stride == op.element_size)
      {
        // Tight on both ends — one copy, no per-call barrier (batched).
        score::gfx::copyBuffer(
            *rhi, cb, op.src, dst,
            op.vertex_count * op.element_size,
            op.src_offset, op.dst_offset,
            score::gfx::BufferCopyBarrier::None);
      }
      else
      {
        // Strided on either end — the src slot size or the dst slot size
        // differs from the element size. Per-vertex copy of
        // min(src_stride, element_size) bytes: the overlap between the two
        // layouts (e.g. tight vec3 src (12 B) → padded-vec4 MDI slot (16 B)
        // → copy the 12 B of real data into each slot's low bytes;
        // zero-fill from uploadStaticBuffer covers the trailing padding).
        // A wider DESTINATION slot is what the interleaved per-instance
        // array needs: translation writes bytes [0,16) of each 32-byte slot
        // and color writes [16,32), so neither may advance by element_size.
        const int per_vertex
            = std::min(src_stride, op.element_size);
        regions.clear();
        regions.reserve(op.vertex_count);
        for(int v = 0; v < op.vertex_count; ++v)
        {
          regions.push_back(
              {op.src_offset + v * src_stride,
               op.dst_offset + v * dst_stride,
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

    // Copies run before the geometry_spec hand-off so the destination MDI
    // buffers are populated before the downstream rasterizer reads them. Gated
    // on renderer.frame: the buffers are shared, so one batch per frame serves
    // every consumer.
    if(m_lastGpuCopiesFrame != renderer.frame)
    {
      issuePendingGpuCopies(renderer, commands);
      m_lastGpuCopiesFrame = renderer.frame;
    }

    // Snapshot last frame's world_transforms into the prev buffer with a pure
    // GPU copy, then apply this frame's per-slot writes through the
    // post-snapshot resource-update batch: the copy reads
    // m_worldTransformsBuffer at its frame N-1 contents, and the next
    // beginPass sees current = frame N, prev = frame N-1.
    //
    // Gated on renderer.frame because runInitialPasses fires once per outgoing
    // edge. A QRhiCommandBuffer pointer cannot serve as the token: every
    // backend's currentFrameCommandBuffer returns the address of one by-value
    // member, so it is constant across frames.
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

  // Single outlet: the concatenated MDI geometry. Scene-wide UBOs and SSBOs
  // ride along as auxiliary_buffer entries, the per-channel material texture
  // arrays and the environment skybox as auxiliary_texture entries; consumer
  // shaders bind them by name through try_bind_from_geometry and
  // try_bind_texture_from_geometry.
  output.push_back(new Port{this, {}, Types::Geometry, {}});
}

ScenePreprocessorNode::~ScenePreprocessorNode() = default;

NodeRenderer* ScenePreprocessorNode::createRenderer(RenderList& /*r*/) const noexcept
{
  return new RenderedScenePreprocessorNode{*this};
}

}
