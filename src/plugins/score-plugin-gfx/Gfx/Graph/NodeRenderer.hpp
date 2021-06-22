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

  virtual void init(RenderList& renderer) = 0;
  virtual void update(RenderList& renderer, QRhiResourceUpdateBatch& res) = 0;

  virtual void runInitialPasses(
      RenderList&,
      QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res,
      Edge& edge);

  [[nodiscard]]
  virtual QRhiResourceUpdateBatch* runRenderPass(
      RenderList&,
      QRhiCommandBuffer& commands,
      Edge& edge);

  virtual void release(RenderList&) = 0;
};

/**
 * @brief Generic renderer.
 *
 * Used for the common case of a single pass node with a vertex & fragment shader,
 * samplers, and a single render target.
 */
class SCORE_PLUGIN_GFX_EXPORT GenericNodeRenderer
    : public score::gfx::NodeRenderer
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

  //TextureRenderTarget m_rt;

  std::vector<Sampler> m_samplers;

  // Pipeline
  ossia::small_vector<std::pair<Edge*, Pipeline>, 2> m_p;

  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_processUBO{};

  DefaultShaderMaterial m_material;
  int64_t materialChangedIndex{-1};

  // Render loop
  void defaultMeshInit(RenderList& renderer, const Mesh& mesh);
  void defaultUBOInit(RenderList& renderer);
  void defaultPassesInit(RenderList& renderer, const Mesh& mesh);
  void init(RenderList& renderer) override;

  void defaultUBOUpdate(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;

  void defaultRelease(RenderList&);
  void release(RenderList&) override;

  void defaultRenderPass(
      RenderList&,
      const Mesh& mesh,
      QRhiCommandBuffer& commands,
      Edge& edge);

  QRhiResourceUpdateBatch* runRenderPass(
      RenderList&,
      QRhiCommandBuffer& commands,
      Edge& edge) override;

};

}
