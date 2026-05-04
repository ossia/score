#pragma once
#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/GpuTiming.hpp>
#include <Gfx/Graph/Node.hpp>

#include <ossia/detail/hash_map.hpp>

#include <memory>

namespace Gfx
{
class AssetTable;
}
namespace score::gfx
{

class GpuResourceRegistry;
class OutputNode;
class VertexFallbackPool;
/**
 * @brief List of nodes to be rendered to an output.
 *
 * This references all the score::gfx::Node that have an effect on a given output,
 * and manages all the matching renderers, as well as a few shared data, such
 * as output-specific UBOs, shared textures and buffers, etc.
 *
 * The score::gfx::Graph creates one RenderList per OutputNode in the graph.
 */
class SCORE_PLUGIN_GFX_EXPORT RenderList
{
  friend struct Graph;
private:
  std::shared_ptr<RenderState> m_state;

public:
  explicit RenderList(OutputNode& output, const std::shared_ptr<RenderState>& state);
  ~RenderList();

  /**
   * @brief Initialize data for this renderer.
   */
  void init();

  /**
   * @brief Initial resource update batch.
   *
   * See QRhiResourceUpdateBatch::merge for documentation on this
   */
  [[nodiscard]] QRhiResourceUpdateBatch* initialBatch() const noexcept;

  /**
   * @brief Store a resource update batch to be submitted on the next render frame.
   *
   * Used by incremental edge additions that happen after the first render frame
   * (when the original m_initialBatch has already been consumed).
   */
  void setInitialBatch(QRhiResourceUpdateBatch* batch) noexcept { m_initialBatch = batch; }

  /**
   * @brief Create buffers for a mesh and mark them for upload.
   *
   * The meshes used by the nodes are cached
   * (as most are just rendering on a full-screen triangle, which we can reuse).
   */
  MeshBuffers initMeshBuffer(const Mesh& mesh, QRhiResourceUpdateBatch& res);

  /**
   * @brief Update / upload this RenderList's shared data
   */
  void update(QRhiResourceUpdateBatch& res);

  /**
   * @brief Render every node in order.
   */
  void render(QRhiCommandBuffer& commands, bool force = false);

  /**
   * @brief Release GPU resources owned by this render list
   */
  void release();

  void releaseBuffer(QRhiBuffer* buf);

  /**
   * @brief Check if the render size has changed in order to rebuild the pipelines.
   */
  bool maybeRebuild(bool force = false);

  /**
   * @brief Obtain the texture corresponding to an output port.
   *
   * This is done by looking for the render target which corresponds to a given port.
   */
  TextureRenderTarget renderTargetForOutput(const Edge& edge) const noexcept;

  /**
   * @brief Look up the render target for a given input port.
   *
   * All input render targets are created centrally before any node init()
   * so that they are available regardless of initialization order.
   */
  TextureRenderTarget renderTargetForInputPort(const Port& p) const noexcept;

  BufferView bufferForInput(const Edge& edge) const noexcept;
  BufferView bufferForOutput(const Edge& edge) const noexcept;

  /**
   * @brief Adapts an image to the GPU limits / format
   *
   * e.g. Y direction, texture size limits...
   */
  QImage adaptImage(const QImage& in);

  /**
   * @brief Output node to which this RenderList is rendering to
   */
  OutputNode& output;

  /**
   * @brief RenderState corresponding to this RenderList
   */
  RenderState& state;

  using Buffers = std::pair<const Mesh* const, MeshBuffers>;
  Buffers acquireMesh(
      const ossia::geometry_spec&, QRhiResourceUpdateBatch& res, const Mesh* current,
      MeshBuffers currentbufs) noexcept;

  /**
   * @brief Nodes present in this RenderList, in order
   */
  std::vector<score::gfx::Node*> nodes;

  /**
   * @brief Renderers - one per node.
   */
  std::vector<score::gfx::NodeRenderer*> renderers;

  /** @brief Clear the renderers so that they get reinitialized on the next frame */
  void clearRenderers();

  /**
   * @brief Texture to use when a texture is missing (2D)
   */
  QRhiTexture& emptyTexture() const noexcept { return *m_emptyTexture; }

  /**
   * @brief Texture to use when a 3D (sampler3D) texture is missing
   */
  QRhiTexture& emptyTexture3D() const noexcept { return *m_emptyTexture3D; }

  /**
   * @brief Texture to use when a cubemap (samplerCube) is missing
   */
  QRhiTexture& emptyTextureCube() const noexcept { return *m_emptyTextureCube; }

  /**
   * @brief Texture to use when a 2D array (sampler2DArray) is missing
   */
  QRhiTexture& emptyTextureArray() const noexcept { return *m_emptyTextureArray; }

  /**
   * @brief UBO corresponding to the output parameters:
   *
   *  - Render size
   *  - Per-API adjustments and globals
   */
  QRhiBuffer& outputUBO() const noexcept { return *m_outputUBO; }

  /**
   * @brief Per-RenderList GPU arena store for scene-graph source nodes.
   *
   * Returns a reference to the registry that owns the Camera / Light /
   * Material / PerDraw arena buffers. Source nodes (Camera, Light,
   * PBRMesh, …) allocate a slot from this registry at construction and
   * write their packed bytes into it at their own update().
   *
   * Valid between init() and release().
   */
  GpuResourceRegistry& registry() noexcept { return *m_registry; }
  const GpuResourceRegistry& registry() const noexcept { return *m_registry; }

  /**
   * @brief Per-RenderList pool of neutral fallback vertex buffers for
   *        "REQUIRED: false" VERTEX_INPUTS whose upstream geometry does
   *        not provide a matching attribute.
   *
   * Valid between init() and release(). See VertexFallbackPool.hpp.
   */
  VertexFallbackPool& vertexFallbackPool() noexcept { return *m_vertexFallbackPool; }

  /**
   * @brief Per-RenderList GPU-timing collector.
   *
   * Renderers wrap their begin/endPass regions in `ScopedGpuTimer` to
   * attribute the CB-wide lastCompletedGpuTime to the named pass. The
   * result is one frame stale — see GpuTiming.hpp for details.
   *
   * The S6 observability panel reads `gpuTimings().snapshot()` on its
   * UI tick and displays per-pass rolling means.
   */
  GpuTimings& gpuTimings() noexcept { return m_gpuTimings; }
  const GpuTimings& gpuTimings() const noexcept { return m_gpuTimings; }

  /**
   * @brief Session-wide asset decode cache.
   *
   * Set by Graph::createRenderList from GfxContext's AssetTable.
   * May be null on test RenderLists or after teardown. Consumers
   * must guard.
   *
   * Plan 09 S1: one decode per asset per session; preprocessor's
   * texture-decode path checks this first, falls back to decode +
   * stage otherwise.
   */
  Gfx::AssetTable* assetTable() const noexcept { return m_assetTable; }
  void setAssetTable(Gfx::AssetTable* t) noexcept { m_assetTable = t; }

  /**
   * @brief A quad mesh correct for this API
   */
  const score::gfx::Mesh& defaultQuad() const noexcept;

  /**
   * @brief A triangle mesh correct for this API
   */
  const score::gfx::Mesh& defaultTriangle() const noexcept;

  /**
   * @brief Whether this list of rendering actions requires depth testing at all
   * 
   * e.g. it's not needed if we're just doing some generative shaders.
   */
  bool requiresDepth(const score::gfx::Port& p) const noexcept;
  bool anyNodeRequiresDepth() const noexcept { return m_requiresDepth; }

  int samples() const noexcept { return m_samples; }

  bool canRender() const noexcept { return m_ready; }

  QSize renderSize(const Edge* e) const noexcept;

  int64_t frame = 0;

  void createAllInputRenderTargets();

  /**
   * @brief Mark this render list as fully built.
   *
   * Prevents maybeRebuild() from unnecessarily tearing down and
   * recreating all resources on the first render frame after
   * createRenderList() has already fully initialized everything.
   */
  void markBuilt() noexcept { m_built = true; m_lastSize = state.renderSize; }

  /// Notify that an edge was removed. Notifies renderers, releases RT if unused.
  ///
  /// @param preserveSinks Optional set of sink Ports that should keep their
  ///   input render target even if this edge was their only feed. Used by
  ///   batched edge updates (see GfxContext::incrementalEdgeUpdate) so that
  ///   inserting a filter between two nodes doesn't destroy and immediately
  ///   re-allocate the same RT when the old and new edges share a sink port.
  void
  onEdgeRemoved(Edge& edge, const ossia::hash_set<const Port*>* preserveSinks = nullptr);

  /// Remove the render target for a specific input port.
  void removeInputRenderTarget(const Port* port);

  /**
   * @brief Resolve the downstream render target size for a node.
   *
   * Returns the maximum size across all downstream render targets that
   * this node renders to. Used as fallback when a node's input port
   * has no explicit render target size.
   */
  QSize resolveDownstreamSize(
      const Node* node,
      const ossia::small_flat_map<const Port*, RenderTargetSpecs, 16>& resolvedSpecs)
      const noexcept;

private:
  OutputUBO m_outputUBOData;

  QRhiResourceUpdateBatch* m_initialBatch{};

  // Scene-graph arena store (camera / light / material / per_draw buffers).
  // Owned by this RenderList; lifetime matches init/release.
  std::unique_ptr<GpuResourceRegistry> m_registry;

  // Pool of tiny shared vertex buffers used to satisfy "REQUIRED: false"
  // VERTEX_INPUTS whose upstream geometry is missing an attribute.
  // Same lifetime as m_registry.
  std::unique_ptr<VertexFallbackPool> m_vertexFallbackPool;

  // GPU-timing collector. Lives as long as the RenderList — outlives
  // individual renderers so per-pass measurements survive node churn.
  GpuTimings m_gpuTimings;

  // Session-wide asset decode cache. Non-owning; GfxContext is the
  // owner. May be null.
  Gfx::AssetTable* m_assetTable{};

  // Material
  QRhiBuffer* m_outputUBO{};
  QRhiTexture* m_emptyTexture{};
  QRhiTexture* m_emptyTexture3D{};
  QRhiTexture* m_emptyTextureCube{};
  QRhiTexture* m_emptyTextureArray{};

  /**
   * @brief Cache of vertex buffers.
   */
  ossia::flat_map<Mesh*, MeshBuffers> m_vertexBuffers;

  ossia::flat_map<ossia::geometry_spec, Mesh*> m_customMeshCache;

  /**
   * @brief Centralized render targets for all image input ports.
   *
   * Created before any node init() so that they are available
   * regardless of initialization order (fixes delayed-edge feedback).
   */
  ossia::small_flat_map<const Port*, TextureRenderTarget, 8> m_inputRenderTargets;

  /**
   * @brief Last size used by this renderer.
   */
  QSize m_lastSize{};

  int m_minTexSize{};
  int m_maxTexSize{};
  int m_samples{1};

  bool m_requiresDepth{};
  bool m_ready{};
  bool m_built{};
};
}
