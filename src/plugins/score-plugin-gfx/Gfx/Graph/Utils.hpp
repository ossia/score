#pragma once
#include <Process/Dataflow/CableData.hpp>

#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Scale.hpp>
#include <Gfx/Graph/Uniforms.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/small_flat_map.hpp>

#include <score_plugin_gfx_export.h>

#include <span>

namespace isf
{
struct descriptor;
}

namespace score::gfx
{
class Node;
class NodeModel;
struct Port;
class VertexFallbackPool;
struct Edge;
class RenderList;
/**
 * @brief Stores a sampler and the texture currently associated with it.
 *
 * `fallback` is the view-type-matched empty texture to bind when `texture`
 * becomes null (no upstream, feedback-loop short, disconnect race). It MUST
 * be one of `RenderList::emptyTexture() / emptyTexture3D() / emptyTextureCube()
 * / emptyTextureArray()` so the bound view type matches the shader's
 * sampler declaration. Leaving this null is only safe for plain sampler2D
 * slots — a samplerCube / sampler3D / sampler2DArray slot with a null
 * `fallback` will trip Vulkan viewType validation or, if the fallback
 * path upstream also produced null, crash with a VK_NULL_HANDLE descriptor
 * write.
 */
struct Sampler
{
  QRhiSampler* sampler{};
  QRhiTexture* texture{};
  QRhiTexture* fallback{};
};

/**
 * @brief Data model for audio data being sent to the GPU
 */
struct AudioTexture
{
  ossia::hash_map<RenderList*, Sampler> samplers;

  std::vector<float> data;
  int channels{};
  int fixedSize{0};
  enum Mode
  {
    Waveform,
    FFT,
    Histogram
  } mode{};

  // Optional sampler config. Empty strings keep legacy defaults
  // (linear / clamp_to_edge). Populated by ISFNode from the parsed
  // audio_input::sampler (FILTER / WRAP). Useful for FFT reads where
  // NEAREST filtering avoids smearing adjacent bins.
  std::string filter;
  std::string wrap;
};

/**
 * @brief Port of a score::gfx::Node
 */
struct Port
{
  //! Parent node of the port
  score::gfx::Node* node{};

  //! Pointer to the corresponding data.
  void* value{};

  //! Type of the value
  Types type{};

  //! Optional setting flags
  Flag flags{};

  //! Edges connected to that port.
  std::vector<Edge*> edges;
};

/**
 * @brief Connection between two score::gfx::Port
 */
struct Edge
{
  Edge(Port* source, Port* sink, Process::CableType t)
      : source{source}
      , sink{sink}
      , type{t}
  {
    source->edges.push_back(this);
    sink->edges.push_back(this);
  }

  ~Edge()
  {
    if(auto it = std::find(source->edges.begin(), source->edges.end(), this);
       it != source->edges.end())
      source->edges.erase(it);
    if(auto it = std::find(sink->edges.begin(), sink->edges.end(), this);
       it != sink->edges.end())
      sink->edges.erase(it);
  }

  Port* source{};
  Port* sink{};
  Process::CableType type{};
};

/**
 * @brief Useful abstraction for storing a graphics pipeline and associated resource bindings.
 */
struct Pipeline
{
  QRhiGraphicsPipeline* pipeline{};
  QRhiShaderResourceBindings* srb{};

  void release()
  {
    if(pipeline)
      pipeline->deleteLater();
    pipeline = nullptr;

    if(srb)
      srb->deleteLater();
    srb = nullptr;
  }
};

/**
 * @brief Useful abstraction for storing all the data related to a render target.
 */
struct TextureRenderTarget
{
  QRhiTexture* texture{};                              // Primary color attachment (location 0)
  std::vector<QRhiTexture*> additionalColorTextures;   // MRT: locations 1..N
  QRhiRenderBuffer* colorRenderBuffer{};
  QRhiRenderBuffer* depthRenderBuffer{};
  QRhiTexture* depthTexture{};                         // Sampleable depth (alternative to depthRenderBuffer)
  QRhiTexture* msDepthTexture{};                       // MSAA depth attachment when depthTexture is the resolve target
  QRhiRenderPassDescriptor* renderPass{};
  QRhiRenderTarget* renderTarget{};

  // A 1×1 color texture allocated when the backend requires a color attachment
  // but the user only wants depth-only rendering. Owned by this RT.
  QRhiTexture* dummyColorTexture{};

  // Number of array layers on `texture` (1 = non-layered, >1 = texture array).
  int arrayLayers{1};

  // Multiview view count (0/1 = disabled).
  int multiViewCount{0};

  // Which layer of `texture`/`additionalColorTextures` this RT renders to.
  // -1 = not applicable (non-layered, or MultiView handles it automatically).
  int renderLayer{-1};

  operator bool() const noexcept { return texture != nullptr || dummyColorTexture != nullptr || depthTexture != nullptr; }

  int colorAttachmentCount() const noexcept
  {
    if(texture)
      return 1 + (int)additionalColorTextures.size();
    if(dummyColorTexture)
      return 1;
    return 0;
  }

  // Returns the actual MSAA sample count of this render target, or -1 if it
  // cannot be determined from the stored fields. Callers must treat -1 as
  // "unknown — fall back to the renderlist's global sample count".
  //
  // Lookup priority:
  //   1. colorRenderBuffer (owned MSAA attachment — always authoritative).
  //   2. texture (single-sample resolve target OR non-MSAA render target).
  //   3. depthTexture (depth-only RTs).
  //   4. msDepthTexture (MSAA depth attachment when depth resolve is used).
  //   5. renderTarget — BUT only when this RT genuinely owns its attachments
  //      (colorRenderBuffer/texture/depthTexture set). A "bare" RT that only
  //      carries renderTarget + renderPass (e.g. a swap-chain wrapper
  //      returned by QRhiSwapChain::currentFrameRenderTarget()) is NOT
  //      queried because swap-chain render-target objects lazily write
  //      their sampleCount only when createOrResize() runs — any read before
  //      that returns the default 1, which would silently mismatch a
  //      multi-sample renderPassDescriptor and produce
  //      VUID-VkGraphicsPipelineCreateInfo-multisampledRenderToSingleSampled-06853.
  //   6. Otherwise return -1 so the caller uses RenderList::samples(), which
  //      IS authoritative for externally-managed swap-chain RTs (it drove
  //      the swap-chain sample count in the first place).
  int sampleCount() const noexcept
  {
    if(colorRenderBuffer)
      return colorRenderBuffer->sampleCount();
    if(texture)
      return texture->sampleCount();
    if(msDepthTexture)
      return msDepthTexture->sampleCount();
    if(depthTexture)
      return depthTexture->sampleCount();
    // renderTarget alone without any owned attachment = swap-chain wrapper.
    // Its sampleCount is unreliable pre-createOrResize; fall through.
    return -1;
  }

  void release()
  {
    if(texture || dummyColorTexture || depthTexture)
    {
      // Use deleteLater() for all GPU resources: Qt RHI commands are async
      // and resources may still be referenced by in-flight frames until
      // endFrame() completes. deleteLater() defers actual destruction to
      // the next beginFrame().
      if(texture)
        texture->deleteLater();
      texture = nullptr;

      if(dummyColorTexture)
        dummyColorTexture->deleteLater();
      dummyColorTexture = nullptr;

      for(auto* t : additionalColorTextures)
        t->deleteLater();
      additionalColorTextures.clear();

      if(colorRenderBuffer)
        colorRenderBuffer->deleteLater();
      colorRenderBuffer = nullptr;

      if(depthRenderBuffer)
        depthRenderBuffer->deleteLater();
      depthRenderBuffer = nullptr;

      if(depthTexture)
        depthTexture->deleteLater();
      depthTexture = nullptr;

      if(msDepthTexture)
        msDepthTexture->deleteLater();
      msDepthTexture = nullptr;

      if(renderPass)
        renderPass->deleteLater();
      renderPass = nullptr;

      if(renderTarget)
        renderTarget->deleteLater();
      renderTarget = nullptr;
    }
  }
};

/**
 * @brief Image data and metadata.
 */
struct Image
{
  QString path;
  std::vector<QImage> frames;
};

/**
 * @brief Create a render target from a texture.
 */
SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget
createRenderTarget(const RenderState& state, QRhiTexture* tex, int samples, bool depth, bool samplableDepth = false);

/**
 * @brief Create a render target from a texture format and size.
 *
 * This function will also create a texture.
 */
SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget createRenderTarget(
    const RenderState& state, QRhiTexture::Format fmt, QSize sz, int samples, bool depth,
    bool samplableDepth = false, QRhiTexture::Flags flags = {});

/**
 * @brief Create a render target with multiple color attachments and optional sampleable depth.
 *
 * @param colorTextures All color attachment textures (must be non-empty, first becomes primary)
 * @param depthTexture Sampleable depth texture, or nullptr for no depth / renderbuffer depth
 */
SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget createRenderTarget(
    const RenderState& state,
    std::span<QRhiTexture* const> colorTextures,
    QRhiTexture* depthTexture,
    int samples);

/**
 * @brief Create a depth-only render target.
 *
 * Allocates a sampleable depth texture (samplableDepth=true) or a depth
 * renderbuffer. If the backend rejects color-less render targets, a 1x1
 * RGBA8 dummy color texture is allocated and stored in the
 * TextureRenderTarget::dummyColorTexture field (owned by the RT).
 *
 * The resulting TextureRenderTarget has:
 *   - `depthTexture` or `depthRenderBuffer` set (never both)
 *   - `texture` == nullptr (depth-only semantics)
 *   - `dummyColorTexture` may be non-null on some backends
 */
SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget createDepthOnlyRenderTarget(
    const RenderState& state, QSize sz, int samples, bool samplableDepth = true,
    QRhiTexture::Format depthFmt = QRhiTexture::D32F);

/**
 * @brief Create a render target that targets a single layer of a texture array.
 *
 * colorTextureArray must have been created with QRhiTexture::TextureArray
 * and at least (renderLayer + 1) layers.
 *
 * depthTexture may be a regular 2D texture (shared across layers) or nullptr
 * to skip depth (use a renderbuffer instead via createRenderTarget overloads).
 */
SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget createLayeredRenderTarget(
    const RenderState& state, QRhiTexture* colorTextureArray, int renderLayer,
    QRhiTexture* depthTexture, int samples);

/**
 * @brief Create a multiview render target (single RT drawing N views at once).
 *
 * colorTextureArray must be a TextureArray with at least multiViewCount layers.
 * depthTextureArray may be nullptr for no depth, or a TextureArray with the
 * same layer count.
 *
 * Requires state.caps.multiview == true — caller must check.
 */
SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget createMultiViewRenderTarget(
    const RenderState& state, QRhiTexture* colorTextureArray, int multiViewCount,
    QRhiTexture* depthTextureArray, int samples);

/**
 * @brief Map an ISF/CSF FORMAT string to a QRhiTexture::Format.
 *
 * Supported: rgba8, bgra8, r8, rg8, r16, rg16, r16f, r32f, rgba16f, rgba32f,
 * d16, d24, d24s8, d32f. Unknown / empty strings fall back to the caller's
 * default. Lookup is case-insensitive.
 */
SCORE_PLUGIN_GFX_EXPORT
QRhiTexture::Format parseOutputFormat(
    const std::string& fmt, QRhiTexture::Format fallback) noexcept;

SCORE_PLUGIN_GFX_EXPORT
void replaceBuffer(QRhiShaderResourceBindings&, int binding, QRhiBuffer* newBuffer);
SCORE_PLUGIN_GFX_EXPORT
void replaceSampler(QRhiShaderResourceBindings&, int binding, QRhiSampler* newSampler);
SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(QRhiShaderResourceBindings&, int binding, QRhiTexture* newTexture);

SCORE_PLUGIN_GFX_EXPORT
void replaceBuffer(
    std::vector<QRhiShaderResourceBinding>&, int binding, QRhiBuffer* newBuffer);
SCORE_PLUGIN_GFX_EXPORT
void replaceSampler(
    std::vector<QRhiShaderResourceBinding>&, int binding, QRhiSampler* newSampler);
SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(
    std::vector<QRhiShaderResourceBinding>&, int binding, QRhiTexture* newTexture);

/**
 * @brief Replace a sampler.
 */
SCORE_PLUGIN_GFX_EXPORT
void replaceSampler(
    QRhiShaderResourceBindings&, QRhiSampler* oldSampler, QRhiSampler* newSampler);

/**
 * @brief Replace the texture currently bound to a sampler.
 */
SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(
    QRhiShaderResourceBindings&, QRhiSampler* sampler, QRhiTexture* newTexture);

/**
 * @brief Replace both sampler and texture in a SRC
 */
SCORE_PLUGIN_GFX_EXPORT
void replaceSamplerAndTexture(
    QRhiShaderResourceBindings&, QRhiSampler* oldSampler, QRhiSampler* newSampler,
    QRhiTexture* newTexture);

/**
 * @brief Replace a texture by another in a set of bindings.
 */
SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(
    QRhiShaderResourceBindings& srb, QRhiTexture* old_tex, QRhiTexture* new_tex);
/**
 * @brief Create bindings following the score conventions for shaders and materials.
 */
SCORE_PLUGIN_GFX_EXPORT
QRhiShaderResourceBindings* createDefaultBindings(
    const RenderList& renderer, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings = {});

/**
 * @brief Match a (name, semantic) request to an upstream geometry attribute.
 *
 * Three-stage cascade shared by all shader modes:
 *   1. semantic_key → name_to_semantic → if known, geom.find(semantic).
 *   2. Custom-attribute lookup by `name`.
 *   3. display_name == name fallback (so { NAME: "position", SEMANTIC:
 *      "custom" } still finds the real position attribute when no custom
 *      one shadows it).
 * If `semantic_key` is empty, `name` is used as the semantic key.
 */
SCORE_PLUGIN_GFX_EXPORT
const ossia::geometry::attribute* findGeometryAttribute(
    const ossia::geometry& geom, std::string_view name, std::string_view semantic_key);

/**
 * @brief Remap a pipeline's vertex input layout using semantic matching.
 *
 * Reflects the compiled vertex shader to find each `in` variable, then for
 * each one runs findGeometryAttribute(name, name) — useful when no isf
 * descriptor is around (legacy callers). Returns true on success, false if
 * a required attribute can't be matched.
 */
SCORE_PLUGIN_GFX_EXPORT
bool remapPipelineVertexInputs(
    QRhiGraphicsPipeline& pip, const QShader& vertexShader,
    const ossia::geometry& geom);

/**
 * @brief Same as above, but honours explicit SEMANTIC on each VERTEX_INPUTS
 * entry from the isf descriptor when present.
 */
SCORE_PLUGIN_GFX_EXPORT
bool remapPipelineVertexInputs(
    QRhiGraphicsPipeline& pip, const QShader& vertexShader,
    const ossia::geometry& geom, const isf::descriptor& desc);

// FallbackBindingPlan now lives in its own header so both Utils.hpp and
// CustomMesh.hpp can depend on it without creating an include cycle
// (Utils.hpp depends on Mesh.hpp, which transitively reaches CustomMesh
// consumers). See <Gfx/Graph/VertexFallbackPlan.hpp>.

/**
 * @brief Fallback-aware overload: the strict-matching behaviour of the
 *        overload above, extended so VERTEX_INPUTS entries with
 *        "REQUIRED": false silently resolve to a shared identity buffer
 *        from the pool when their semantic is absent upstream.
 *
 * @p pool     per-RenderList shared fallback buffer pool
 * @p batch    any uploads for freshly-allocated fallback buffers are
 *             recorded here
 * @p outPlan  filled with the bindings the caller must merge into the
 *             draw's QRhiCommandBuffer::VertexInput array. Cleared on
 *             entry.
 *
 * Returns false (and logs which input failed) if:
 *   - a REQUIRED=true input has no matching upstream attribute, OR
 *   - a REQUIRED=false input has no matching upstream attribute AND the
 *     declared GLSL TYPE is unsupported (mat4 / integer / sampler) OR
 *     the resolved semantic is not in the whitelist AND no explicit
 *     DEFAULT was supplied.
 */
SCORE_PLUGIN_GFX_EXPORT
bool remapPipelineVertexInputs(
    QRhiGraphicsPipeline& pip, const QShader& vertexShader,
    const ossia::geometry& geom, const isf::descriptor& desc,
    QRhi& rhi, VertexFallbackPool& pool, QRhiResourceUpdateBatch& batch,
    FallbackBindingPlan& outPlan);

/**
 * @brief Create a render pipeline following the score conventions for shaders and materials.
 */
SCORE_PLUGIN_GFX_EXPORT
Pipeline buildPipeline(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings = {});

/**
 * @brief Lower-level buildPipeline variant: bring your own SRB.
 *
 * The returned Pipeline::srb equals the srb you passed — no ownership
 * transfer. Useful when the caller wants to share a pipeline across
 * multiple Passes that each have their own SRB (layout-compatible with
 * this one per QRhi contract); the pipeline's stored SRB is only used
 * for layout extraction at create() time and never dereferenced at draw
 * time.
 */
SCORE_PLUGIN_GFX_EXPORT
Pipeline buildPipeline(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt,
    QRhiShaderResourceBindings* srb);

// Forward declarations — definitions in PipelineStateHelpers.hpp, IsfBindingsBuilder.hpp
} // namespace score::gfx

namespace isf
{
struct sampler_config;
}

namespace score::gfx
{
/**
 * @brief Build a QRhiSampler from an isf::sampler_config.
 *
 * Fields left empty/unset in the config are filled with ossia defaults
 * (linear filtering, no mipmaps, clamp-to-edge). When the config sets a
 * comparison op other than "never", the returned sampler is a shadow
 * comparison sampler.
 *
 * The returned sampler is created (create() was called) and has no name
 * assigned; callers should setName() before or after create() as needed.
 * Ownership follows the standard QRhi convention — callers delete it.
 */
SCORE_PLUGIN_GFX_EXPORT
QRhiSampler* makeSampler(QRhi& rhi, const isf::sampler_config& cfg);
} // namespace score::gfx

namespace isf
{
struct pipeline_state;
}

namespace score::gfx
{
struct GraphicsStorageResources;

/**
 * @brief Create a render pipeline applying pipeline_state from an ISF descriptor.
 *
 * This overload replaces the legacy hardcoded `setDepthTest(true)/setDepthWrite(true)`
 * on RawRaster and the `anyNodeRequiresDepth()` fallback on ISF with a unified
 * path driven by `state`. When `state` is empty (all fields nullopt), behaviour
 * matches the legacy variant exactly for backwards compatibility.
 *
 * `extraBindings` is typically the result of IsfBindingsBuilder::buildExtraBindings().
 * `multiViewCount` >= 2 activates multiview rendering (requires state.caps.multiview).
 *
 * Plan 09 S6: when `useShadingRate == true` AND
 * `renderer.state.caps.variableRateShading == true`, the pipeline
 * gets `QRhiGraphicsPipeline::UsesShadingRate`. The shading-rate
 * texture / per-draw rate itself is supplied elsewhere (via the
 * render-target attachment's `setShadingRateMap` or the command-
 * buffer's `setShadingRate`). Presets opt in; silent no-op when the
 * backend doesn't support VRS.
 */
SCORE_PLUGIN_GFX_EXPORT
Pipeline buildPipelineWithState(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> extraBindings,
    const isf::pipeline_state& state,
    int multiViewCount = 0,
    bool useShadingRate = false);

/**
 * @brief Get a pair of compiled vertex / fragment shaders from GLSL 4.5 sources.
 *
 * Note: this function will throw if a shader is invalid.
 */
SCORE_PLUGIN_GFX_EXPORT
std::pair<QShader, QShader>
makeShaders(const RenderState& v, QString vert, QString frag);

/**
 * @brief Compile a compute shader.
 *
 * Note: this function will throw if the shader is invalid.
 */
SCORE_PLUGIN_GFX_EXPORT
QShader makeCompute(const RenderState& v, QString compt);

/**
 * @brief Utility to represent a shader material following score conventions.
 *
 * The material is synthesized from the input ports.
 */
struct SCORE_PLUGIN_GFX_EXPORT DefaultShaderMaterial
{
  void init(
      RenderList& renderer, const std::vector<Port*>& input,
      ossia::small_vector<Sampler, 8>& samplers);

  QRhiBuffer* buffer{};
  int size{};
};

/**
 * @brief Resize the size of a texture to fit within GPU limits
 */
SCORE_PLUGIN_GFX_EXPORT
QSize resizeTextureSize(QSize img, int min, int max) noexcept;

/**
 * @brief Resize a texture to fit within GPU limits
 */
SCORE_PLUGIN_GFX_EXPORT
QImage resizeTexture(const QImage& img, int min, int max) noexcept;

inline void copyMatrix(const QMatrix4x4& mat, float* ptr) noexcept
{
  memcpy(ptr, mat.constData(), sizeof(float) * 16);
}
inline void copyMatrix(const QMatrix3x3& mat, float* ptr) noexcept
{
  memcpy(ptr, mat.constData(), sizeof(float) * 9);
}

/**
 * @brief Compute the scale to apply to a texture so that it fits in a GL viewport.
 */
SCORE_PLUGIN_GFX_EXPORT
QSizeF computeScaleForMeshSizing(score::gfx::ScaleMode mode, QSizeF viewport, QSizeF texture);

/**
 * @brief Compute the scale to apply to a texture rendered to a quad the size of viewport
 */
SCORE_PLUGIN_GFX_EXPORT
QSizeF computeScaleForTexcoordSizing(
    score::gfx::ScaleMode mode, QSizeF viewport, QSizeF texture);

/**
 * @brief Schedule a Dynamic buffer update when we can guarantee the buffer outlives the frame.
 */
inline void updateDynamicBufferWithStoredData(
    QRhiResourceUpdateBatch* ub
  , QRhiBuffer* buf
  , int offset
  , int64_t bytesize
  , const char* data
  )
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  ub->updateDynamicBuffer(buf, offset, QByteArray::fromRawData(data, bytesize));
#else
  ub->updateDynamicBuffer(buf, offset, bytesize, data);
#endif
}

inline void updateDynamicBufferWithStoredData(
    QRhiResourceUpdateBatch* ub
    , QRhiBuffer* buf
    , int offset
    , QByteArray b
    )
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  ub->updateDynamicBuffer(buf, offset, std::move(b));
#else
  ub->updateDynamicBuffer(buf, offset, b.size(), b.data());
#endif
}

/**
 * @brief Schedule a Static buffer update when we can guarantee the buffer outlives the frame.
 */
inline void uploadStaticBufferWithStoredData(
    QRhiResourceUpdateBatch* ub
    , QRhiBuffer* buf
    , int offset
    , int64_t bytesize
    , const char* data
    )
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  ub->uploadStaticBuffer(buf, offset, QByteArray::fromRawData(data, bytesize));
#else
  ub->uploadStaticBuffer(buf, offset, bytesize, data);
#endif
}

inline void uploadStaticBufferWithStoredData(
    QRhiResourceUpdateBatch* ub
    , QRhiBuffer* buf
    , int offset
    , QByteArray b
    )
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  ub->uploadStaticBuffer(buf, offset, std::move(b));
#else
  ub->uploadStaticBuffer(buf, offset, b.size(), b.data());
#endif
}

SCORE_PLUGIN_GFX_EXPORT
std::vector<Sampler> initInputSamplers(
    const score::gfx::Node& node, RenderList& renderer, const std::vector<Port*>& ports,
    const isf::descriptor* desc = nullptr);
}
