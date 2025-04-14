#pragma once
#include <Gfx/Graph/CommonUBOs.hpp>
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

class OutputNode;
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
   * @brief Texture to use when a texture is missing
   */
  QRhiTexture& emptyTexture() const noexcept { return *m_emptyTexture; }

  /**
   * @brief UBO corresponding to the output parameters:
   *
   *  - Render size
   *  - Per-API adjustments and globals
   */
  QRhiBuffer& outputUBO() const noexcept { return *m_outputUBO; }

  /**
   * @brief A quad mesh correct for this API
   */
  const score::gfx::Mesh& defaultQuad() const noexcept;

  /**
   * @brief A triangle mesh correct for this API
   */
  const score::gfx::Mesh& defaultTriangle() const noexcept;

  int samples() const noexcept { return m_samples; }

  bool canRender() const noexcept { return m_ready; }

  QSize renderSize(const Edge* e) const noexcept;

  int64_t frame = 0;

private:
  OutputUBO m_outputUBOData;

  QRhiResourceUpdateBatch* m_initialBatch{};
  // Material
  QRhiBuffer* m_outputUBO{};
  QRhiTexture* m_emptyTexture{};

  /**
   * @brief Cache of vertex buffers.
   */
  ossia::flat_map<Mesh*, MeshBuffers> m_vertexBuffers;

  ossia::flat_map<ossia::geometry_spec, Mesh*> m_customMeshCache;

  /**
   * @brief Last size used by this renderer.
   */
  QSize m_lastSize{};

  int m_minTexSize{};
  int m_maxTexSize{};
  int m_samples{1};

  bool m_ready{};
  bool m_built{};
};
}
