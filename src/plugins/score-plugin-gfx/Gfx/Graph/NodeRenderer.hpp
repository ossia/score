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

  virtual std::optional<QSize> renderTargetSize() const noexcept = 0;
  virtual TextureRenderTarget renderTarget() const noexcept = 0;
  virtual void init(RenderList& renderer) = 0;
  virtual void update(RenderList& renderer, QRhiResourceUpdateBatch& res) = 0;
  virtual void runPass(
      RenderList&,
      QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch& updateBatch)
      = 0;
  virtual void release(RenderList&) = 0;
  virtual void releaseWithoutRenderTarget(RenderList&) = 0;
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

  virtual ~GenericNodeRenderer() { }

  const NodeModel& node;

  TextureRenderTarget m_rt;

  std::vector<Sampler> m_samplers;

  // Pipeline
  Pipeline m_p;

  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_processUBO{};

  DefaultShaderMaterial m_material;
  int64_t materialChangedIndex{-1};

  friend struct Graph;
  friend struct RenderList;

  TextureRenderTarget createRenderTarget(const RenderState& state);
  TextureRenderTarget renderTarget() const noexcept override { return m_rt; }

  std::optional<QSize> renderTargetSize() const noexcept override;
  // Render loop
  virtual void customInit(RenderList& renderer);
  void init(RenderList& renderer) override;

  virtual void
  customUpdate(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override;

  virtual void customRelease(RenderList&);
  void release(RenderList&) override;
  void releaseWithoutRenderTarget(RenderList&) override;

  void runPass(
      RenderList&,
      QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch& updateBatch) override;

  QRhiGraphicsPipeline* pipeline() const { return m_p.pipeline; }
  QRhiShaderResourceBindings* resources() const { return m_p.srb; }
};

}
