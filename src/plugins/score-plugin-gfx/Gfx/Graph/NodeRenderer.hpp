#pragma once
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

/**
 * @brief Renderer for a given node.
 */
class SCORE_PLUGIN_GFX_EXPORT NodeRenderer
{
public:
  explicit NodeRenderer() noexcept;
  virtual ~NodeRenderer();

  virtual TextureRenderTarget renderTargetForInput(const Port& input) = 0;

  //! Called when all the inbound nodes to a texture input have finished rendering.
  //! Mainly useful to slip in a readback.
  virtual void
  inputAboutToFinish(RenderList& renderer, const Port& p, QRhiResourceUpdateBatch*&);

  virtual void init(RenderList& renderer) = 0;
  virtual void update(RenderList& renderer, QRhiResourceUpdateBatch& res) = 0;

  virtual void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge);

  virtual void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge);

  virtual void release(RenderList&) = 0;
};

using PassMap = ossia::small_vector<std::pair<Edge*, Pipeline>, 2>;
SCORE_PLUGIN_GFX_EXPORT
void defaultPassesInit(
    PassMap& passes, const std::vector<Edge*>& edges, RenderList& renderer,
    const Mesh& mesh, const QShader& v, const QShader& f, QRhiBuffer* processUBO,
    QRhiBuffer* matUBO, const std::vector<Sampler>& samplers);

SCORE_PLUGIN_GFX_EXPORT
void defaultRenderPass(
    QRhiBuffer* meshBuffer, QRhiBuffer* idxBuffer, RenderList& renderer,
    const Mesh& mesh, QRhiCommandBuffer& cb, Edge& edge, PassMap& passes);

SCORE_PLUGIN_GFX_EXPORT
void quadRenderPass(
    QRhiBuffer* meshBuffer, QRhiBuffer* idxBuffer, RenderList& renderer,
    QRhiCommandBuffer& cb, Edge& edge, PassMap& passes);

/**
 * @brief Generic renderer.
 *
 * Used for the common case of a single pass node with a vertex & fragment shader,
 * samplers, and a single render target.
 */
class SCORE_PLUGIN_GFX_EXPORT GenericNodeRenderer : public score::gfx::NodeRenderer
{
public:
  GenericNodeRenderer(const NodeModel& node) noexcept
      : NodeRenderer{}
      , node{node}
  {
  }

  TextureRenderTarget renderTargetForInput(const Port& p) override;
  virtual ~GenericNodeRenderer() { }

  const NodeModel& node;
  std::vector<Sampler> m_samplers;

  QShader m_vertexS;
  QShader m_fragmentS;

  // Pipeline
  PassMap m_p;

  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_processUBO{};

  DefaultShaderMaterial m_material;
  int64_t materialChangedIndex{-1};

  // Render loop
  void defaultMeshInit(RenderList& renderer, const Mesh& mesh);
  void processUBOInit(RenderList& renderer);
  void defaultPassesInit(RenderList& renderer, const Mesh& mesh);
  void defaultPassesInit(
      RenderList& renderer, const Mesh& mesh, const QShader& v, const QShader& f);

  void init(RenderList& renderer) override;

  void defaultUBOUpdate(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;

  void defaultRelease(RenderList&);
  void release(RenderList&) override;

  void defaultRenderPass(
      RenderList&, const Mesh& mesh, QRhiCommandBuffer& commands, Edge& edge);

  void defaultRenderPass(
      RenderList&, const Mesh& mesh, QRhiCommandBuffer& commands, Edge& edge,
      PassMap& passes);

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;
};

}
