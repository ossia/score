#pragma once

#include <score_plugin_gfx_export.h>

#include <ossia/dataflow/geometry_port.hpp>  // ossia::gpu_slot_ref
#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/hash_map.hpp>

#ifndef OFFSETALLOCATOR_HPP_2026_04_24
#define OFFSETALLOCATOR_HPP_2026_04_24
#include <offsetAllocator.hpp>
#endif

#include <QtGui/private/qrhi_p.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace score::gfx
{
class RenderList;

/**
 * @brief Per-RenderList arena store for GPU-resident scene data.
 *
 * Owns one QRhiBuffer per well-known arena kind (camera UBO, light SSBO,
 * material SSBO, per-draw SSBO, …) and hands out offset-based slots via
 * a fixed-stride free-list. Source nodes (Camera, Light, PBRMesh, …) hold
 * a slot for their lifetime and write their packed bytes into it at
 * their own `update()`; the preprocessor binds the registry's buffers as
 * scene auxiliaries. No CPU→GPU work happens in the preprocessor's render
 * path — every upload is gated to a source-node message.
 *
 * Currently covers scalar UBO / SSBO arenas only. Texture-array layer
 * allocation (baseColorArray, metalRoughArray, …) stays inside the
 * existing ScenePreprocessor::ChannelState for now; it will migrate into
 * this registry later.
 *
 * Lifetime: created on RenderList::init, destroyed on RenderList::release.
 * Not thread-safe — all calls must come from the render thread.
 */
class SCORE_PLUGIN_GFX_EXPORT GpuResourceRegistry
{
public:
  // Well-known arenas. Size tables live in GpuResourceRegistry.cpp and match the
  // packed GPU layouts in SceneGPUState.hpp and CameraMath.hpp. Every entry
  // implies a QRhiBuffer allocation at init time.
  //
  // The Raw* arenas are written by source halp nodes at their own operator()()
  // time: view-independent, aspect-ratio-agnostic, pre-composition. The cooked
  // arenas are populated by ScenePreprocessor's transform passes, which combine
  // Raw inputs with the render target's aspect ratio and the scene-graph parent
  // chain, and are what consumer shaders bind. Material and Env have no
  // composition dependency, so source nodes write their cooked slot directly.
  enum class Arena : uint8_t
  {
    // ── Shared / source-authored ──────────────────────────────────
    // These arenas hold view- and filter-independent bytes: every
    // preprocessor reads the same data regardless of its camera /
    // render target / upstream scene filtering. The producer owns the
    // slot; multiple preprocessors consume via gpu_slot_ref + isLive().
    RawCamera,        // RawCameraData      — 64 B per slot, UBO
    RawLight,         // RawLightData       — 64 B per slot, SSBO
    RawTransform,     // RawLocalTransform  — 64 B per slot, SSBO
    Material,         // MaterialGPU        — 64 B per slot, SSBO
    Env,              // EnvParamsUBO       — 64 B per slot, UBO

    // Cooked outputs (camera UBOs, composed world matrices, per-draw structs,
    // LightGPU with world direction, MaterialGPU with resolved textureRefs) are
    // preprocessor-private: they are view- and filter-dependent, so a shared arena
    // would be wrong when two preprocessors see different filtered views.

    Count_
  };

  // Fixed-stride slot: the arena buffer is a packed array of stride-byte slots,
  // slot i at byte offset i * stride. The slot index is the arena-level identity
  // consumer shaders use, e.g. scene_materials.entries[slot_index]. Allocation is
  // O(1) through a free-list stack. Trades OffsetAllocator's variable-size
  // flexibility for a shader-indexable layout and a 1:1 mapping between
  // internal_index and byte offset, so arena reads need no translation table.
  struct Slot
  {
    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    Arena arena{Arena::RawCamera};
    uint32_t slot_index{kInvalidIndex};
    uint32_t size{0};        // requested payload size (≤ arena stride)
    uint32_t generation{};   // stamped on allocate; bumps on free

    bool valid() const noexcept { return slot_index != kInvalidIndex; }
  };

  GpuResourceRegistry() = default;
  GpuResourceRegistry(const GpuResourceRegistry&) = delete;
  GpuResourceRegistry& operator=(const GpuResourceRegistry&) = delete;
  ~GpuResourceRegistry();

  /**
   * @brief Create the arena buffers. Must be called before any allocate().
   *
   * Per-arena capacity is fixed at init time (grow-in-place reallocation
   * is a follow-up). If an arena runs out of room, allocate() returns
   * an invalid Slot and logs a warning.
   *
   * Persist-across-rebuild contract: the registry now lives on the
   * OutputNode and survives RenderList rebuilds (e.g. viewport resize).
   * The owning OutputNode lazy-calls init() exactly once for a given
   * QRhi lifetime. Subsequent createRenderList calls reuse the registry
   * as-is (texture arrays, mesh slabs, arena slot generations all
   * preserved). Use isInitialized() to detect "registry already up".
   */
  void init(QRhi& rhi, QRhiResourceUpdateBatch& batch);

  /**
   * @brief True if init() has been called and destroyOwned()/destroy()
   * has not. Used by RenderList::init to gate the (otherwise asserting)
   * init() call when the registry is being reused across an RL rebuild.
   */
  bool isInitialized() const noexcept { return m_rhi != nullptr; }

  /**
   * @brief QRhi this registry was init()'d against. Null when not
   * initialised. The owning OutputNode uses this to decide whether
   * the registry is still bound to its QRhi (vs. a fresh QRhi created
   * after a setSwapchainFormat-style teardown).
   */
  QRhi* boundRhi() const noexcept { return m_rhi; }

  /**
   * @brief Seed reserved arena slots with sensible defaults.
   *
   * Called by the owning RenderList after init() and after the initial
   * resource-update batch is ready. Currently writes a default
   * white-dielectric MaterialGPU into Material arena slot 0 — the slot
   * `arenaSlotForMaterial(nullptr)` returns when a draw has no
   * material assigned (e.g. a Primitive cube with the user never
   * having dropped a Material node on it). Without this seed, slot 0
   * carries whatever bytes the previous registered material left
   * behind, producing the confusing "every unmaterialed mesh is red
   * because the first registered material was red" symptom.
   *
   * Idempotent — second call is a no-op once @c m_defaults_seeded is
   * set.
   */
  void seedDefaults(QRhiResourceUpdateBatch& batch);

  /**
   * @brief Destroy the arena buffers via the owning RenderList.
   *
   * Every arena QRhiBuffer is routed through @c RenderList::releaseBuffer
   * so the RenderList's bookkeeping sees the release and any other path
   * that still holds a pointer to the buffer can't accidentally double-
   * free it. Prefer this overload; call it from RenderList::release()
   * before the QRhi teardown.
   */
  void destroy(RenderList& renderer);

  /**
   * @brief Destructor fallback — buffers are nulled without touching the
   * QRhi. Only safe when @ref destroy(RenderList&) has already run (or
   * when the QRhi has already torn them down as children). Leaks the
   * QRhiBuffer wrappers otherwise; that's the lesser evil vs. a
   * use-after-free in the common "QRhi already dead" path.
   */
  void destroy();

  /**
   * @brief Tear down arena buffers + texture arrays + mesh streams
   * directly (no RenderList plumbing). Called by the owning OutputNode
   * when its QRhi is about to be destroyed (destroyOutput, ~OutputNode).
   *
   * Persist-across-rebuild contract: the registry survives across RL
   * rebuilds (RenderList::release is a no-op for the registry now), so
   * the QRhi-routed teardown that used to happen in destroy(RenderList&)
   * has no live RenderList to run through any more. We `delete` the
   * QRhiBuffer / QRhiTexture / QRhiSampler wrappers directly: the QRhi
   * is still alive at this call site (callers MUST invoke this BEFORE
   * RenderState::destroy() / setSwapchainFormat-style teardown), so the
   * destructors free both the wrapper and the underlying GPU resource
   * cleanly. After this call the registry is back to its pre-init()
   * state and can be re-init()'d against a new QRhi.
   */
  void destroyOwned();

  /**
   * @brief Reserve a slot in the given arena for @p size bytes.
   * @return invalid Slot on OOM. Caller must check Slot::valid().
   */
  Slot allocate(Arena arena, uint32_t size);

  /**
   * @brief Return the slot to the free list. Safe to call with invalid Slot.
   */
  void free(Slot& slot);

  /**
   * @brief Buffer underlying an arena. Null until init().
   *
   * Downstream consumers (preprocessor, rasterizer SRBs) bind this buffer
   * with the slot offset + size from Slot.
   */
  QRhiBuffer* buffer(Arena arena) const noexcept;

  /**
   * @brief Byte offset of a slot inside its arena's buffer.
   */
  uint32_t slotOffset(const Slot& slot) const noexcept;

  /**
   * @brief Byte stride of the arena — every slot is this many bytes.
   * Consumer shaders index `arena.entries[slot_index]` where entries[]
   * has std430 stride equal to this value.
   */
  uint32_t arenaSlotStride(Arena arena) const noexcept;

  /**
   * @brief Slot capacity of the arena (number of slots, not bytes).
   */
  uint32_t arenaSlotCount(Arena arena) const noexcept;

  /**
   * @brief Upload @p size bytes starting at @p data into a slot.
   *
   * Thin wrapper around `QRhiResourceUpdateBatch::updateDynamicBuffer`
   * (for Dynamic-usage arenas) or `uploadStaticBuffer` (Static).
   * Called by source nodes in their `update()` when their content
   * changes — never per frame for unchanged data.
   */
  void updateSlot(
      QRhiResourceUpdateBatch& res, const Slot& slot, const void* data,
      uint32_t size) noexcept;

  /**
   * @brief Produce an ossia::gpu_slot_ref that can be stamped on a
   * scene-graph component for the downstream preprocessor to consume.
   *
   * The returned ref captures (arena tag, offset, size, internal slot
   * index, generation). The preprocessor uses isLive() to validate it
   * before reading GPU bytes.
   */
  ossia::gpu_slot_ref toOssiaRef(const Slot& slot) const noexcept
  {
    if(!slot.valid())
      return {};
    ossia::gpu_slot_ref r;
    r.arena = (uint32_t)slot.arena;
    r.offset = slotOffset(slot);
    r.size = slot.size;
    r.internal_index = slot.slot_index;
    r.generation = slot.generation;
    return r;
  }

  /**
   * @brief Return true if the ref still points at a live allocation.
   *
   * O(1): one array access + one uint32 compare. The generation table
   * is bumped on every allocate() and free(), so a ref from a prior
   * allocation at the same slot index fails the compare.
   */
  bool isLive(const ossia::gpu_slot_ref& r) const noexcept
  {
    if(r.arena >= (uint32_t)Arena::Count_ || r.size == 0)
      return false;
    const auto& a = m_arenas[r.arena];
    if(r.internal_index >= a.slot_generations.size())
      return false;
    return a.slot_generations[r.internal_index] == r.generation;
  }

  // ─── Material texture arrays ──────────────────────────────────────
  //
  // Per-channel static texture arrays shared across every preprocessor in this
  // RenderList. Static textures dedup by texture_source pointer, so every producer
  // referencing an asset gets the same layer; dynamic handles (video textures,
  // runtime GPU outputs) get per-slot bindings in `dynamicTextures`, named
  // <channel>Dyn<slot> in consumer shaders.
  //
  // Sharing is safe because a texture belongs to an asset or a wired GPU handle,
  // independent of which preprocessor is looking.

  enum class TextureChannel : uint8_t
  {
    BaseColor  = 0,
    MetalRough = 1,
    Normal     = 2,
    Emissive   = 3,
    Occlusion  = 4,  // Separate glTF occlusionTexture (when distinct from MR).
    Count_     = 5
  };

  // Default layer size + max dynamic slots. Matched across channels so
  // samplers are interchangeable and consumer shaders can declare a
  // fixed sampler count.
  static constexpr int kTextureLayerSize = 1024;
  // 4 slots: high enough for scenes that legitimately use 3-4 distinct dynamic
  // textures per channel (multi-camera capture, layered video), while 4 channels
  // x 4 slots plus the static arrays and skybox/IBL stays under the
  // 16-samplers-per-stage RHI floor. LRU eviction covers the rest.
  static constexpr int kMaxDynamicSlots  = 4;

  // Per-channel static buckets, each holding textures of one (format, pixelSize)
  // tuple. Consumer shaders declare N sampler2DArrays per channel and switch on
  // the bucket field decoded from MaterialGPU::textureRefs.
  //
  // The cap of 16 keeps 5 channels x 16 buckets plus ~10 dynamic slots at about 90
  // samplers per pipeline, well inside Vulkan's default combined-image-sampler
  // pool budget. Real scenes need 1-3 buckets per channel, and buckets are
  // allocated lazily as uploads discover new (format, size) combinations.
  //
  // The tex_ref_static encoding reserves a 7-bit bucket field, so the cap can grow
  // to 128 without changing the packed layout or the shader decode masks -- but
  // the shader sampler arrays in classic_pbr_full.frag must be enlarged to match,
  // and the descriptor pool budget re-checked. GLES 3.1 / WebGL 2 guarantee only
  // 16 textures per stage and would need a reduced-bucket preset.
  static constexpr int kMaxBuckets = 16;

  /**
   * @brief Channel texture state with multi-bucket support.
   *
   * The MaterialGPU::textureRefs[] encoding is
   * `source:2 | bucket:7 | layer:23` — the 7-bit bucket field
   * addresses up to 128 distinct (format, pixelSize) tuples in the
   * encoding; the runtime cap is kMaxBuckets (currently 16). Currently
   * only ONE bucket is kept live per channel: same behaviour as the
   * pre-refactor single-array path, shaders unchanged. Lifting the cap
   * requires the preprocessor to allocate a new bucket when a texture
   * of a new (format, pixelSize) appears, and shipped shaders to grow
   * a bucket-switch ladder in sample_slot().
   *
   * The Bucket struct holds everything that used to be at channel
   * scope (QRhiTexture*, layers, layerMap) plus the discriminating
   * (format, pixelSize) tuple. Dynamic (runtime-GPU) slots stay at
   * channel scope — they carry opaque QRhiTexture*s with no
   * canonical format/size, so no sensible bucket to live in.
   */
  struct TextureChannelState
  {
    struct Bucket
    {
      QRhiTexture* array{};          // QRhiTexture::TextureArray + channel flags
      QRhiTexture::Format format{QRhiTexture::RGBA8};
      QSize pixelSize;               // all layers in a bucket share this size
      int layers{};                  // current layer count

      // Per-bucket sampler config. Bucket key extended to include this:
      // distinct (format, size, sampler_config) tuples land in distinct
      // buckets so per-glTF-texture wrap/filter modes are honoured even
      // when multiple materials share a channel array.
      ossia::texture_sampler_config sampler_config{};
      QRhiSampler* sampler{};        // created on first allocation; owned

      // Dedup: texture_source shared_ptr pointer → layer index in
      // this bucket's `array`. Append-only within a materials list;
      // cleared when the list changes.
      ossia::flat_map<const ossia::texture_source*, int> layerMap;
    };

    // Currently buckets.size() <= 1; may grow up to kMaxBuckets.
    std::vector<Bucket> buckets;

    // Dynamic (runtime-GPU) slot map, keyed by QRhiResource::globalResourceId()
    // rather than the raw QRhiTexture* -- the allocator recycles freed pointer
    // values, and qrhivulkan.cpp documents the same hazard for SRB tracking, so a
    // recycled address must look like a fresh resource here too.
    //
    // Slots are recycled by LRU: when the map is full and a new id arrives, the slot
    // with the smallest dynamicSlotLastUse is evicted. lastUse is bumped on every
    // access, so the evicted slot is the one no live material references.
    ossia::flat_map<quint64, int> dynamicSlotMap;
    std::vector<QRhiTexture*>     dynamicTextures;       // slot idx → texture
    std::vector<uint64_t>         dynamicSlotLastUse;    // slot idx → access counter at last lookup
    uint64_t                      dynamicSlotCounter{0}; // monotonic, bumped on each resolve
    // Value of dynamicSlotCounter at the previous sweepStaleDynamicTextureSlots()
    // pass. A slot whose dynamicSlotLastUse is <= this was not re-resolved by any
    // live material since, so its stored QRhiTexture* is orphaned and must be
    // cleared before it can be bound.
    uint64_t                      dynamicSweepCheckpoint{0};

    // Compatibility shims. Callers that haven't been updated to loop over
    // buckets[] go through these for legacy single-bucket semantics.
    // Returns null / 0 when no bucket has been allocated yet.
    QRhiTexture* primaryArray() const noexcept
    {
      return buckets.empty() ? nullptr : buckets[0].array;
    }
    int primaryLayers() const noexcept
    {
      return buckets.empty() ? 0 : buckets[0].layers;
    }

    // Access or lazily create bucket 0 with an owned (format, size).
    // Kept for init-time fallback allocation only — production code
    // goes through findOrCreateBucket() which selects the right bucket
    // for the texture's actual (format, size).
    Bucket& ensurePrimary(QRhiTexture::Format fmt, QSize sz)
    {
      if(buckets.empty())
        buckets.emplace_back();
      auto& b = buckets[0];
      b.format = fmt;
      b.pixelSize = sz;
      return b;
    }

    // Find a bucket matching (fmt, sz), creating one if none matches and kMaxBuckets
    // is not reached. Returns {bucket_index, pointer}, or {-1, nullptr} on overflow,
    // which the caller reports and turns into tex_ref_none. Bucket identity is the
    // exact tuple, no rounding.
    std::pair<int, Bucket*>
    findOrCreateBucket(QRhiTexture::Format fmt, QSize sz)
    {
      for(std::size_t i = 0; i < buckets.size(); ++i)
      {
        if(buckets[i].format == fmt && buckets[i].pixelSize == sz)
          return {(int)i, &buckets[i]};
      }
      if((int)buckets.size() >= kMaxBuckets)
        return {-1, nullptr};
      buckets.emplace_back();
      auto& b = buckets.back();
      b.format = fmt;
      b.pixelSize = sz;
      return {(int)buckets.size() - 1, &b};
    }

    // Sampler-config-aware variant, keyed on (format, pixelSize, sampler_config).
    // Used by the glTF path so a scene mixing wrap modes splits across buckets, each
    // with its own QRhiSampler. Falls back to the 2-tuple variant when the sampler
    // config is the default.
    std::pair<int, Bucket*>
    findOrCreateBucket(
        QRhiTexture::Format fmt, QSize sz,
        const ossia::texture_sampler_config& sampler_cfg)
    {
      for(std::size_t i = 0; i < buckets.size(); ++i)
      {
        if(buckets[i].format == fmt && buckets[i].pixelSize == sz
           && buckets[i].sampler_config == sampler_cfg)
          return {(int)i, &buckets[i]};
      }
      if((int)buckets.size() >= kMaxBuckets)
        return {-1, nullptr};
      buckets.emplace_back();
      auto& b = buckets.back();
      b.format = fmt;
      b.pixelSize = sz;
      b.sampler_config = sampler_cfg;
      return {(int)buckets.size() - 1, &b};
    }
  };

  /**
   * @brief Shared state for one of the four PBR texture channels.
   * Preprocessors / producers read-modify this in place; contents are
   * view-independent (asset identity drives layer assignment) so
   * sharing across preprocessors is correct.
   */
  TextureChannelState& textureChannel(TextureChannel ch) noexcept
  {
    return m_textureChannels[(std::size_t)ch];
  }
  const TextureChannelState& textureChannel(TextureChannel ch) const noexcept
  {
    return m_textureChannels[(std::size_t)ch];
  }

  /**
   * @brief Shader-visible aux-texture name for a channel's static array
   * (`baseColorArray`, `metalRoughArray`, `normalArray`, `emissiveArray`).
   */
  static const char* textureChannelArrayName(TextureChannel ch) noexcept;

  /**
   * @brief Shader-visible aux-texture name base for a channel's dynamic
   * slots (`baseColorDyn`, `metalRoughDyn`, `normalDyn`, `emissiveDyn`).
   * Full name is `<base><slot_index>`, slot_index < kMaxDynamicSlots.
   */
  static const char* textureChannelDynBaseName(TextureChannel ch) noexcept;

  /**
   * @brief QRhiTexture creation flags for a channel. sRGB channels
   * (base color, emissive) get hardware sRGB→linear on sample; MR and
   * normal stay linear.
   */
  static QRhiTexture::Flags textureChannelFlags(TextureChannel ch) noexcept;

  /**
   * @brief Register a runtime GPU texture handle for this channel's
   * dynamic-slot set. Returns the slot index (0 .. kMaxDynamicSlots-1)
   * or -1 if the slot cap is exhausted.
   *
   * Slot assignment is persistent across frames — once a handle is in
   * the map, it keeps its slot until the registry is destroyed. This
   * ordering-free property lets multiple producers AND the
   * preprocessor all call resolveDynamicSlot concurrently within a
   * frame and agree on the same answer for the same handle.
   *
   * The ~6-handle cap (4 channels × kMaxDynamicSlots ≈ 8 slots
   * registry-wide) is fine for the common case of 1-2 live
   * per-channel dynamic textures; more elaborate eviction (LRU,
   * explicit release from producer teardown) is a future concern
   * when the first real 3+-handle scene shows up.
   */
  int resolveDynamicSlot(TextureChannel channel, void* native_handle) noexcept;

  // ─── Mesh arena manager ───────────────────────────────────────────
  //
  // Per-mesh slab allocator over the 5 attribute streams of the MDI concatenated
  // geometry -- positions, normals, texcoords, tangents, indices -- each a single
  // growth-capped QRhiBuffer.
  //
  // Indirect-draw correctness invariant: one baseVertex is applied to ALL vertex
  // bindings (VkDrawIndexedIndirectCommand::vertexOffset), so per-mesh byte
  // offsets must satisfy pos/nrm/tan = baseVertex * 16 and uv = baseVertex * 8.
  // Independent per-stream allocators cannot guarantee that once alloc/free
  // traffic fragments the streams: they pick free blocks from different size bins
  // and the offsets diverge, so the vertex shader reads attributes from the wrong
  // slab.
  //
  // Hence TWO shared allocators: m_vertexAllocator in vertex units and
  // m_indexAllocator in index units, both capped at 8M slots. Each slab carries
  // one vertex_slot and one index_slot, and per-stream byte offsets derive as
  // vertex_slot.offset * stride and index_slot.offset * 4, so lockstep is
  // structural. Indirect draw takes baseVertex = vertex_slot.offset and
  // firstIndex = index_slot.offset.
  //
  // Cache: a stable_id hit reuses the slab and skips the upload; a miss allocates.
  // The sweep frees slabs unseen for `grace` frames.
  //
  // Backing buffers are pointer-stable for the registry's lifetime, so downstream
  // bindings resolve once: 128 MB each for positions, normals and tangents, 64 MB
  // texcoords, 32 MB indices.

  enum class MeshStream : uint8_t
  {
    Positions  = 0,
    Normals    = 1,
    Texcoords  = 2,  // TEXCOORD_0 (primary UV).
    Tangents   = 3,
    Colors     = 4,  // glTF COLOR_0, vec4 (vec3 sources padded with alpha=1).
    Texcoords1 = 5,  // glTF TEXCOORD_1 (lightmap / secondary UV).
    Indices    = 6,
    Count_     = 7
  };

  // Bytes per element per stream, matching the MDI output layout the rasterizer
  // presets consume: positions and normals are vec3 padded to vec4 for std430,
  // tangents and colors vec4 (vec3 sources padded with alpha=1), texcoords vec2,
  // indices uint32.
  static constexpr uint32_t kMeshStride[(std::size_t)MeshStream::Count_]
      = {16, 16, 8, 16, 16, 8, 4};

  // Capacity reserved per stream at init: 128 MB for positions, normals, tangents
  // and colors (16 B stride, 8M verts), 64 MB for each texcoord stream (8 B),
  // 32 MB for indices (4 B). A scene exceeding these gets a sentinel allocation
  // from allocate() and the caller skips the mesh.
  static constexpr uint32_t kMeshCapBytes[(std::size_t)MeshStream::Count_]
      = {
          128u * 1024u * 1024u,
          128u * 1024u * 1024u,
           64u * 1024u * 1024u,
          128u * 1024u * 1024u,
          128u * 1024u * 1024u,  // colors
           64u * 1024u * 1024u,  // texcoords1
           32u * 1024u * 1024u,
  };

  /**
   * @brief Slab handle returned by MeshArenaManager::acquire.
   *
   * One per mesh (keyed on stable_id). Holds ONE vertex-unit allocation
   * (shared across positions / normals / texcoords / tangents) and ONE
   * index-unit allocation. Per-stream byte offsets are derived in
   * meshSlabOffsetBytes() as `vertex_slot.offset * stride` /
   * `index_slot.offset * 4`. This guarantees baseVertex consistency
   * across all vertex bindings even after fragmentation — see the
   * "CRITICAL invariant" block above.
   *
   * `last_seen_frame` is bumped each frame the owner calls
   * markSeen(); sweep() frees slabs whose last_seen is older than
   * `current_frame - grace`. Grace = FramesInFlight + 1 is the
   * safe default (let in-flight draws finish).
   */
  struct MeshSlab
  {
    uint64_t stable_id{};
    OffsetAllocator::Allocation vertex_slot{};  // offset/size in vertex units
    OffsetAllocator::Allocation index_slot{};   // offset/size in index  units
    uint32_t vertex_count{};
    uint32_t index_count{};
    uint32_t last_seen_frame{};
    bool freshly_allocated{};  // true on the frame the slab was created
  };

  /// Acquire a slab for a mesh: an existing slab on stable_id hit, a fresh
  /// allocation on miss, nullptr on allocator exhaustion. `freshly_allocated` on
  /// the returned slab means the caller must upload the bytes through
  /// uploadMeshStream().
  ///
  /// `current_frame` stamps a real release frame on the count-mismatch grace-queue
  /// enqueue; without it every deferred release would be freed on the next sweep,
  /// defeating the guard against in-flight draws.
  MeshSlab* acquireMeshSlab(
      uint64_t stable_id,
      uint32_t vertex_count,
      uint32_t index_count,
      uint32_t current_frame) noexcept;

  /// Mark a slab as seen this frame so sweep() doesn't reclaim it.
  void markMeshSlabSeen(uint64_t stable_id, uint32_t current_frame) noexcept;

  /// Release slabs whose `last_seen_frame < current_frame - grace`.
  /// Grace defaults to 2 (covers FramesInFlight+1 on typical backends).
  void sweepMeshSlabs(uint32_t current_frame, uint32_t grace = 2) noexcept;

  /// Clear dynamic texture slots not re-resolved by any live material since the
  /// previous sweep, so the consumer's "bind every non-null dynamic slot" loop can
  /// never bind a dangling pointer. Must run once per frame after the
  /// resolveDynamicSlot pass and before the slots are bound; the call site inside
  /// sweepMeshSlabs satisfies that, since ScenePreprocessor::update() runs
  /// rebuildChannel before rebuildMDI().
  void sweepStaleDynamicTextureSlots() noexcept;

  /// Free pending-release slabs whose released_frame + grace <= current_frame.
  /// Called by sweepMeshSlabs and by acquireMeshSlab before its fresh allocate, so
  /// a count-mismatch whose previous slot has served its grace recycles that
  /// capacity in the same update() instead of a spurious "pool exhausted". Leaves
  /// the *SlotsUsed trackers alone: those are decremented eagerly at enqueue time.
  void drainExpiredPendingReleases(
      uint32_t current_frame, uint32_t grace = 2) noexcept;

  /// Explicit release of a slab by stable_id (used on scene teardown).
  /// The release is enqueued into the pending-releases grace queue and freed
  /// from the OffsetAllocator only after `grace` frames have elapsed, matching
  /// the same contract as sweepMeshSlabs. Pass the current render-frame counter
  /// so the sweep can determine when it is safe to reclaim the sub-allocation.
  void releaseMeshSlab(uint64_t stable_id, uint32_t current_frame) noexcept;

  /// Byte offset of a stream within its backing buffer. Use directly
  /// as `uploadStaticBuffer(buf, offset, size, data)`.
  uint32_t meshSlabOffsetBytes(
      const MeshSlab& slab, MeshStream stream) const noexcept;

  /// Backing QRhiBuffer for a stream. Stable pointer across the
  /// registry's lifetime (pre-sized, never grown).
  QRhiBuffer* meshStreamBuffer(MeshStream s) const noexcept;

  /// Upload CPU bytes into a slab's stream. Thin wrapper around
  /// QRhiResourceUpdateBatch::uploadStaticBuffer at the slab's
  /// computed offset.
  void uploadMeshStream(
      QRhiResourceUpdateBatch& res, const MeshSlab& slab,
      MeshStream s, const void* data, uint32_t size) noexcept;

  /// Total bytes in use per stream (for the telemetry panel).
  uint32_t meshStreamUsedBytes(MeshStream s) const noexcept;
  uint32_t meshStreamFreeBytes(MeshStream s) const noexcept;

private:
  struct ArenaState
  {
    QRhiBuffer* buffer{};
    uint32_t slot_stride{0};   // bytes per slot (arena layout is a packed
                               // std430-compatible array of this stride)
    uint32_t slot_count{0};    // total slots (capacity_bytes = stride × count)
    QRhiBuffer::UsageFlags usage{};
    QRhiBuffer::Type type{QRhiBuffer::Dynamic};

    // LIFO stack of free slot indices. Push on free, pop on allocate.
    // O(1) alloc / free, no fragmentation (every slot is the same size).
    std::vector<uint32_t> free_slots;

    // Per-slot generation, indexed by slot_index. Sized to slot_count
    // at init() and bumped on every allocate()/free() to that slot.
    // Consumers check the stamped generation in their gpu_slot_ref via
    // isLive().
    std::vector<uint32_t> slot_generations;
  };

  std::array<ArenaState, (std::size_t)Arena::Count_> m_arenas{};

  std::array<TextureChannelState, (std::size_t)TextureChannel::Count_>
      m_textureChannels{};

  // Per-stream backing buffers, one QRhiBuffer per attribute. Allocation is not
  // per-stream: a single m_vertexAllocator hands out vertex-unit slots that all
  // four vertex streams interpret through their own stride, and m_indexAllocator
  // handles indices, keeping byte offsets in lockstep for baseVertex.
  struct MeshStreamState
  {
    QRhiBuffer* buffer{};
    uint32_t capacity_bytes{};
    QRhiBuffer::UsageFlags usage{};
  };
  std::array<MeshStreamState, (std::size_t)MeshStream::Count_> m_meshStreams{};

  // Shared vertex / index allocators (slot units, not bytes).
  // capacity_slots = min(stream_capacity_bytes / stream_stride) across
  // the four vertex streams = 8M for the default sizes; index pool
  // capacity = 8M slots.
  std::unique_ptr<OffsetAllocator::Allocator> m_vertexAllocator;
  std::unique_ptr<OffsetAllocator::Allocator> m_indexAllocator;
  uint32_t m_vertexSlotsCapacity{};
  uint32_t m_indexSlotsCapacity{};
  uint32_t m_vertexSlotsUsed{};
  uint32_t m_indexSlotsUsed{};

  ossia::hash_map<uint64_t, MeshSlab> m_meshSlabs;

  // Slabs whose `released_frame` is set are waiting out the grace
  // period before their OffsetAllocator allocations return to the
  // free list. Prevents use-after-free when an in-flight draw still
  // references the old offset.
  struct PendingRelease
  {
    uint64_t stable_id{};
    uint32_t released_frame{};
    OffsetAllocator::Allocation vertex_slot{};
    OffsetAllocator::Allocation index_slot{};
  };
  std::vector<PendingRelease> m_pendingReleases;

  QRhi* m_rhi{};

  // Set by seedDefaults() after writing the default-MaterialGPU bytes
  // into Material arena slot 0. Idempotent guard so repeated calls are
  // free.
  bool m_defaults_seeded{false};
};

} // namespace score::gfx
