#pragma once

// Shared infrastructure for binding `storage_input` and `csf_image_input`
// declarations into a graphics pipeline's shader resource bindings.
//
// Mirrors the pattern established by RenderedCSFNode (for compute) but wired
// to Vertex|Fragment stages for ISF / Raw Raster Pipeline / Scene Pass nodes.

#include <Gfx/Graph/Utils.hpp>
#include <isf.hpp>

#include <QtGui/private/qrhi_p.h>

#include <score_plugin_gfx_export.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace score::gfx
{

/**
 * @brief One SSBO attached to a graphics pipeline.
 *
 * Covers:
 *  - user-declared storage_input's (shader-visible read-only / read-write SSBOs)
 *  - persistent ping-pong pairs (current + previous frame)
 *  - indirect-draw argument buffers (BUFFER_USAGE="indirect_draw")
 *  - auxiliary buffers that travel with the geometry (forwarded from upstream)
 */
struct GraphicsSSBO
{
  std::string name;        //!< Base GLSL identifier (e.g. "particles")
  std::string access;      //!< "read_only" / "write_only" / "read_write"
  std::string buffer_usage;//!< "", "indirect_draw", "indirect_draw_indexed"
  bool persistent{false};  //!< Ping-pong swapped every frame
  bool owned{true};        //!< This SSBO owns `buffer` and `prev` (releases them)
  int64_t size{0};         //!< Buffer size in bytes (0 = auto from layout)

  // Layout fields (for size computation + validation). May be empty for auxiliaries.
  std::vector<isf::storage_input::layout_field> layout;

  // Buffer handles. `buffer` is the currently-written slot (R/W for persistent).
  // `prev` is only set when persistent — holds the previous frame's data (R/O).
  QRhiBuffer* buffer{};
  QRhiBuffer* prev{};

  // Resolved SRB binding slots.
  int binding{-1};       //!< Binding of `buffer`
  int prev_binding{-1};  //!< Binding of `prev` (only set when persistent)

  // Stages that see this binding (fragment / vertex / both).
  QRhiShaderResourceBinding::StageFlags stages{};

  // Optional: indices into the Node's input/output port vectors. -1 = not
  // connected to a port (e.g. private aux buffer or persistent-only).
  int input_port_index{-1};
  int output_port_index{-1};
};

/**
 * @brief One storage image attached to a graphics pipeline.
 */
struct GraphicsStorageImage
{
  std::string name;
  std::string access; //!< "read_only" / "write_only" / "read_write"
  std::string format; //!< e.g. "rgba8", "r32f", "r32ui"
  bool is3D{false};
  bool cubemap{false};    //!< imageCube — 6-layer cubemap storage image
  bool is_array{false};   //!< image2DArray — N-layer array texture
  bool persistent{false}; //!< Ping-pong two textures swapped every frame
  int depth{0};   //!< Explicit Z dimension for 3D textures; 0 = use default (16)
  int layers{0}; //!< Layer count for is_array (0 = use parser-supplied default)

  QRhiTexture* texture{}; //!< Current (write / read_write) slot
  QRhiTexture* prev{};    //!< Previous frame (read-only); only set when persistent
  bool owned{true};

  int binding{-1};
  int prev_binding{-1};   //!< Binding of `prev` (only set when persistent)
  QRhiShaderResourceBinding::StageFlags stages{};

  int input_port_index{-1};
  int output_port_index{-1};
};

/**
 * @brief One UBO sourced from an upstream Buffer port (uniform_input).
 *
 * Bound via QRhiShaderResourceBinding::uniformBuffer (std140) rather than
 * the SSBO bufferLoad/bufferStore used for storage_input.
 */
struct GraphicsUBO
{
  std::string name;
  QRhiBuffer* buffer{};
  bool owned{false};      //!< Always false for now: borrowed from upstream.
  int binding{-1};
  QRhiShaderResourceBinding::StageFlags stages{};
  int input_port_index{-1};
};

/**
 * @brief Aggregate of all graphics-visible storage resources for a node.
 */
struct GraphicsStorageResources
{
  std::vector<GraphicsSSBO> ssbos;
  std::vector<GraphicsStorageImage> images;
  std::vector<GraphicsUBO> ubos;

  // Quick aliases: first SSBO with BUFFER_USAGE="indirect_draw*". Populated
  // by collectGraphicsStorageResources. Updated by callers when the underlying
  // SSBO's buffer pointer changes (e.g. when an upstream CSF rebuilds it).
  QRhiBuffer* indirectDrawBuffer{};
  bool indirectDrawIndexed{false};
  int indirectDrawSsboIndex{-1};

  // Next free binding index after all graphics-visible storage resources
  // (SSBOs + images + UBOs) have been assigned by
  // collectGraphicsStorageResources. This is exactly the value libisf's
  // isf_emit_graphics_storage() returns (isf.cpp:3406-3449) and the binding
  // at which the codegen places the multiview UBO (isf.cpp:3773-3783). Callers
  // that append a multiview UBO MUST use this rather than re-deriving a max
  // over ssbos/images alone — that omission ignored uniform_input UBOs and
  // collided the multiview binding with the last UBO's slot. -1 until the
  // first collectGraphicsStorageResources() call.
  int nextBinding{-1};

  // Sentinel zero-buffer bound when an SSBO/UBO upstream port disconnects
  // mid-session. QRhi (especially Vulkan) requires every SRB binding to
  // point at a valid resource — without a sentinel, a disconnect leaves
  // the binding pointing at a dangling QRhiBuffer* (the prior upstream's
  // buffer, which was deleteLater'd when the upstream node was destroyed).
  // Lazily allocated on first disconnect, sized to the largest binding
  // observed (kSentinelSize). Single buffer reused for both SSBO and UBO
  // disconnects since the descriptor type is set on the SRB binding side,
  // not the buffer side; QRhi accepts a buffer with both StorageBuffer and
  // UniformBuffer usage flags. owned=true; freed in release().
  QRhiBuffer* sentinelBuffer{};
  uint32_t sentinelSize{0};

  void release()
  {
    for(auto& s : ssbos)
    {
      if(s.owned)
      {
        if(s.buffer) s.buffer->deleteLater();
        if(s.prev)   s.prev->deleteLater();
      }
      s.buffer = nullptr;
      s.prev = nullptr;
    }
    ssbos.clear();

    for(auto& i : images)
    {
      if(i.owned)
      {
        if(i.texture) i.texture->deleteLater();
        if(i.prev) i.prev->deleteLater();
      }
      i.texture = nullptr;
      i.prev = nullptr;
    }
    images.clear();

    for(auto& u : ubos)
    {
      if(u.owned && u.buffer)
        u.buffer->deleteLater();
      u.buffer = nullptr;
    }
    ubos.clear();

    if(sentinelBuffer)
    {
      sentinelBuffer->deleteLater();
      sentinelBuffer = nullptr;
    }
    sentinelSize = 0;

    indirectDrawBuffer = nullptr;
    indirectDrawSsboIndex = -1;
    nextBinding = -1;
  }
};

// --- API ------------------------------------------------------------------

/**
 * @brief Walk desc.inputs once and populate `out` with the storage buffers
 *        and images declared by the shader.
 *
 * Bindings are assigned sequentially starting from `firstBinding`. Persistent
 * SSBOs consume TWO consecutive bindings.
 *
 * No GPU resources are allocated here — call ensureStorageResources() later.
 */
SCORE_PLUGIN_GFX_EXPORT
void collectGraphicsStorageResources(
    const isf::descriptor& desc, int firstBinding, GraphicsStorageResources& out);

/**
 * @brief Create missing buffers and textures.
 *
 * Safe to call every frame — idempotent. Resizes buffers when they don't match
 * the current layout. For persistent SSBOs, allocates both the current and
 * prev buffers. For indirect-draw buffers, adds the IndirectBuffer usage flag.
 */
SCORE_PLUGIN_GFX_EXPORT
void ensureStorageResources(
    QRhi& rhi, QRhiResourceUpdateBatch& res, const RenderList& renderer,
    const isf::descriptor& desc, GraphicsStorageResources& store,
    QSize renderSize);

/**
 * @brief Produce the QRhiShaderResourceBinding list for the graphics pipeline.
 *
 * Call this from inside addOutputPass() after buildPipeline() has been set up.
 * The result is concatenated with the standard bindings (sampler, material,
 * processUBO, etc.) via the `additionalBindings` span in createDefaultBindings.
 */
SCORE_PLUGIN_GFX_EXPORT
QVarLengthArray<QRhiShaderResourceBinding, 8> buildExtraBindings(
    const GraphicsStorageResources& store);

/**
 * @brief Wire read-only SSBOs to upstream geometry buffers.
 *
 * When a storage_input is declared as `read_only` AND the upstream node
 * supplies a buffer on the port, the binding is rewired to point at the
 * upstream's QRhiBuffer (no allocation needed). Called each frame to track
 * port changes.
 */
SCORE_PLUGIN_GFX_EXPORT
void bindUpstreamBuffers(
    RenderList& renderer, const std::vector<Port*>& inputPorts,
    GraphicsStorageResources& store,
    QRhiShaderResourceBindings* srb = nullptr);

/**
 * @brief Swap current/prev for all persistent SSBOs and storage images,
 *        then update the SRB.
 *
 * Call at end of frame, after all passes have run. Symmetric to the existing
 * texture ping-pong in RenderedISFNode (the `swap(passes, altPasses)` at
 * RenderedISFNode.cpp:782).
 */
SCORE_PLUGIN_GFX_EXPORT
void swapPersistentSSBOs(
    GraphicsStorageResources& store, QRhiShaderResourceBindings& srb);

/**
 * @brief Swap current/prev pointers in `store` without touching any SRB.
 *
 * Used by multi-pass / multi-SRB renderers that need to apply the same
 * post-swap state to many descriptor sets: call this once per frame, then
 * call reapplyStorageBindings on every affected SRB. Calling
 * swapPersistentSSBOs per-SRB would double-swap and cancel out.
 */
SCORE_PLUGIN_GFX_EXPORT
void swapPersistentSSBOsState(GraphicsStorageResources& store);

/**
 * @brief Re-apply the current persistent-storage state to a single SRB.
 *
 * Pairs with swapPersistentSSBOsState: after swapping `store` once, call
 * this on every SRB that references the persistent bindings so the
 * descriptor set matches the new pointers. Uses replaceBuffer's
 * updateResources() fast path — no srb->create() rebuild — to avoid
 * thrashing the SRB pool slot every frame on a static scene (the
 * cf4b7d6f5 / diag-211 fix removed the trailing create() that earlier
 * versions of this function called).
 */
SCORE_PLUGIN_GFX_EXPORT
void reapplyStorageBindings(
    const GraphicsStorageResources& store, QRhiShaderResourceBindings& srb);

/**
 * @brief Wire read-only csf_image_input storage images to an upstream
 *        geometry's published auxiliary_textures.
 *
 * Symmetric to `bindUpstreamBuffers` for SSBOs: when a csf_image_input is
 * declared `read_only` AND the upstream geometry publishes a storage image
 * with the same name (e.g. an upstream CSF wrote to it via image_input
 * with `write_only`/`read_write`), this swaps the storage image's texture
 * pointer to the upstream's published handle and frees the auto-allocated
 * placeholder we created in `ensureStorageResources`.
 *
 * Without this, every read_only csf_image_input INPUTS in a downstream
 * RawRaster / ISF stage reads from its OWN zero-initialised texture instead
 * of the upstream's actual contents — silently broken.
 *
 * Called per-frame; idempotent. When `srb` is non-null, patches the binding
 * in-place via `replaceTexture`. The lookup is purely by name match against
 * `geometry.auxiliary_textures` (the same name-match pattern used by
 * RawRaster's `m_auxTextureSamplers` rebind path).
 */
SCORE_PLUGIN_GFX_EXPORT
void bindUpstreamImagesFromGeometry(
    GraphicsStorageResources& store, const ossia::geometry& geometry,
    QRhiShaderResourceBindings* srb = nullptr);

/**
 * @brief Wire INPUTS storage_input / uniform_input bindings to upstream
 *        geometry's published auxiliary_buffers list (name-match).
 *
 * SSBO/UBO sibling of `bindUpstreamImagesFromGeometry`. ScenePreprocessor
 * publishes scene_lights / world_transforms / per_draws / scene_materials /
 * scene_counts / scene_light_indices / camera UBO / env UBO as named aux
 * buffers travelling along the geometry edge — flattened-scene shaders
 * (classic_pbr et al.) declare matching INPUTS storage_input/uniform_input
 * blocks and the runtime resolves them by name.
 *
 * Without this, INPUTS storage_input/uniform_input that go through the
 * m_storage path stay at the 16-byte placeholder allocated by
 * `ensureStorageResources` for owned SSBOs — vertices read a zero
 * PerDraw, multiply by a zero world_transforms matrix, and collapse to
 * origin. (Indirect-draw storage_inputs are skipped — they have no shader
 * binding.)
 *
 * For CPU-backed aux buffers a fresh QRhiBuffer is allocated and the data
 * uploaded immediately into `res`; the entry's `owned` flag is updated so
 * `release()` cleans up correctly. For GPU-backed aux buffers we just
 * adopt the upstream handle (`owned=false`).
 *
 * Patches the SRB in-place when a target SRB is provided; idempotent so
 * multi-SRB callers can invoke once per SRB without re-running the lookup.
 */
SCORE_PLUGIN_GFX_EXPORT
void bindUpstreamBuffersFromGeometry(
    QRhi& rhi, QRhiResourceUpdateBatch& res,
    GraphicsStorageResources& store, const ossia::geometry& geometry,
    QRhiShaderResourceBindings* srb = nullptr);

/**
 * @brief Decode an isf::storage_input::visibility string to Qt RHI stage flags.
 *
 * "fragment" → FragmentStage
 * "vertex" → VertexStage
 * "vertex+fragment" / "both" / "graphics" → Vertex | Fragment
 * "compute" → ComputeStage
 * "none" → 0
 */
SCORE_PLUGIN_GFX_EXPORT
QRhiShaderResourceBinding::StageFlags visibilityToStages(std::string_view visibility) noexcept;

/**
 * @brief Byte size of a single GLSL primitive type as used for SSBO element
 *        strides in this codebase.
 *
 * Coverage: scalars (`float`, `int`, `uint`, `bool`), vectors (`vec[234]`,
 * `ivec[234]`, `uvec[234]`), and matrices (`mat2`, `mat3`, `mat4`). Sampler /
 * image / opaque types are not covered (return the fallback). Returns 16 as a
 * fallback for unknown / unsupported types.
 *
 * Conventions:
 *  - Returns 12 for `vec3`/`ivec3`/`uvec3` (the bare component size). Consumers
 *    that need std140 / std430 array stride must align to 16 themselves; for
 *    that case prefer `std430ArrayStride` below, which encapsulates the rule
 *    and keeps the two domains (bare type size vs. stride-in-SSBO) from
 *    drifting at call sites. ISF auxiliary layouts continue to align at the
 *    field level via `std430LayoutSize`.
 *  - `mat2` is reported as 16 (two `vec2` columns, no per-column padding).
 *  - `mat3` is reported as 48 (three `vec4`-padded columns); this matches both
 *    std140 and std430 column-major layout for `mat3` in storage blocks.
 *  - `mat4` is reported as 64.
 *
 * This is the single source of truth for GLSL type → element size in
 * `score-plugin-gfx`; do not introduce private copies (see diagnostic 095).
 *
 * Note: For the vertex-attribute format → byte-size mapping
 * (`ossia::geometry::attribute` enum), see the unrelated helper inside
 * `RenderedCSFNode.cpp`; it operates on a different domain (binary attribute
 * formats, not GLSL type strings).
 */
SCORE_PLUGIN_GFX_EXPORT
int64_t glslTypeSizeBytes(std::string_view type) noexcept;

/**
 * @brief Same as glslTypeSizeBytes, but resolves user-defined types from
 * the descriptor's TYPES section. Falls back to the built-in size table
 * for primitives, then to descriptor.types lookup for struct names. The
 * std430 size of a struct is the sum of its fields' sizes, each rounded
 * up to a 16-byte boundary (matching the array-of-struct alignment rule
 * already used by `std430LayoutSize` for AUXILIARY blocks). Returns 16
 * (the lenient default) for unresolved names.
 */
SCORE_PLUGIN_GFX_EXPORT
int64_t glslTypeSizeBytes(std::string_view type, const isf::descriptor& d) noexcept;

/**
 * @brief Compute the std430 element size of a layout (vector of
 * `{name,type}` field entries), each field rounded up to 16 bytes per
 * the array-of-struct alignment rule. Used by AUXILIARY blocks and by
 * the user-defined struct lookup in glslTypeSizeBytes.
 */
SCORE_PLUGIN_GFX_EXPORT
int64_t std430LayoutSize(
    const std::vector<isf::storage_input::layout_field>& layout) noexcept;

/**
 * @brief std430 array stride for a GLSL primitive type when laid out as
 * `T array[]` inside a shader storage block.
 *
 * Differs from `glslTypeSizeBytes` only for vec3-shaped vectors: per the
 * std430 layout rules, an array of `vec3` (or `ivec3` / `uvec3`) keeps
 * the element's vec4-aligned base alignment, so the per-element stride
 * is 16 bytes — the trailing 4 bytes are padding the GPU does not write
 * but consumer reads must skip. For scalars, vec2, vec4 and matrices,
 * the stride equals the bare type size, so this returns
 * `glslTypeSizeBytes(type)` unchanged.
 *
 * Use this — never `glslTypeSizeBytes` — when sizing a CSF SoA output
 * SSBO buffer or setting a downstream vertex binding stride that mirrors
 * the SSBO's std430 layout. Mixing the two is the source of the silent
 * vec3 corruption diagnosed in the 3DGS pipeline.
 */
SCORE_PLUGIN_GFX_EXPORT
int64_t std430ArrayStride(std::string_view type) noexcept;

/**
 * @brief Same as `std430ArrayStride`, but resolves user-defined struct
 * names against the descriptor's TYPES section. Falls back to
 * `glslTypeSizeBytes(type, d)` for non-vec3 primitives and structs.
 */
SCORE_PLUGIN_GFX_EXPORT
int64_t std430ArrayStride(std::string_view type, const isf::descriptor& d) noexcept;

}
