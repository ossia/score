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

#include <string>
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
  bool persistent{false}; //!< Ping-pong two textures swapped every frame

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

  // Quick aliases: first SSBO with BUFFER_USAGE="indirect_draw*". Populated by
  // collectGraphicsStorageResources. The actual buffer pointer is refreshed
  // each frame via refreshIndirectDrawBuffer.
  QRhiBuffer* indirectDrawBuffer{};
  bool indirectDrawIndexed{false};
  int indirectDrawSsboIndex{-1};

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

    indirectDrawBuffer = nullptr;
    indirectDrawSsboIndex = -1;
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
 * Pairs with swapPersistentSSBOsState: after swapping `store` once, call this
 * on every SRB that references the persistent bindings so the descriptor set
 * matches the new pointers. Calls srb.create() when any binding changed.
 */
SCORE_PLUGIN_GFX_EXPORT
void reapplyStorageBindings(
    const GraphicsStorageResources& store, QRhiShaderResourceBindings& srb);

/**
 * @brief Refresh the indirect-draw buffer pointer from an upstream port.
 *
 * Extracted from RenderedRawRasterPipelineNode::update (lines ~932–957).
 * Used when the indirect draw args come from an upstream CSF/RawRaster
 * geometry_input that writes to an SSBO with BUFFER_USAGE="indirect_draw".
 *
 * Returns true if `store.indirectDrawBuffer` changed — caller should then
 * refresh its MeshBuffers::indirectDrawBuffer.
 */
SCORE_PLUGIN_GFX_EXPORT
bool refreshIndirectDrawBuffer(
    RenderList& renderer, const std::vector<Port*>& inputPorts,
    GraphicsStorageResources& store);

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

}
