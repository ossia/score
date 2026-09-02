#pragma once

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
namespace Gfx
{

class SCORE_PLUGIN_GFX_EXPORT InvertYRenderer final
    : public score::gfx::OutputNodeRenderer
{
public:
  explicit InvertYRenderer(
      const score::gfx::Node& n, score::gfx::TextureRenderTarget rt,
      QRhiReadbackResult& readback);

  score::gfx::TextureRenderTarget m_inputTarget;
  score::gfx::TextureRenderTarget m_renderTarget;

  QShader m_vertexS, m_fragmentS;

  std::vector<score::gfx::Sampler> m_samplers;

  score::gfx::Pipeline m_p;

  score::gfx::MeshBuffers m_mesh{};

  // m_inputTarget is a SNAPSHOT of the owning output node's render target,
  // taken in createRenderer() and refreshed in init(). The output node can
  // replace both the QRhiTextureRenderTarget and the QRhiRenderPassDescriptor
  // behind our back — BackgroundNode::resize() `deleteLater()`s them and
  // installs fresh ones — and when the resize takes the fast path
  // (RenderList::resizeSwapchainSizedTargets) the re-init is deferred to the
  // next render frame. Anything reading the snapshot in that window (an
  // incremental edge add: Graph::createAllMissingPasses ->
  // RenderList::renderTargetForOutput -> here -> addOutputPass's
  // renderPass->serializedFormat()) dereferences freed memory. Re-adopt the
  // node's LIVE target on every query: the output node is the authority, and
  // this is the same rule init() applies.
  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    if(auto* out = dynamic_cast<const score::gfx::OutputNode*>(&this->node))
    {
      auto cur = out->currentRenderTarget();
      if(cur.renderTarget && cur.renderPass)
        m_inputTarget = cur;
    }
    return m_inputTarget;
  }

  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override;

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override;
  void release(score::gfx::RenderList&) override;

  void updateReadback(QRhiReadbackResult& rb) { m_readback = &rb; }

private:
  QRhiReadbackResult* m_readback{};
};

class ScaledRenderer : public score::gfx::OutputNodeRenderer
{
public:
  score::gfx::TextureRenderTarget m_inputTarget;
  score::gfx::TextureRenderTarget m_renderTarget;

  QShader m_vertexS, m_fragmentS;

  std::array<score::gfx::Sampler, 1> m_samplers;
  score::gfx::Pipeline m_p;
  score::gfx::MeshBuffers m_mesh{};

  // When the output is a swap chain, the render target must be re-queried for
  // every frame (QRhiSwapChain::currentFrameRenderTarget: "the value must not
  // be cached and reused between frames"). Null for offscreen outputs.
  QRhiSwapChain* m_swapChain{};


  ScaledRenderer(
      score::gfx::TextureRenderTarget outputTarget, const score::gfx::RenderState& state,
      const score::gfx::Node& parent, QRhiSwapChain* swapChain = nullptr);
  ~ScaledRenderer();

  score::gfx::TextureRenderTarget renderTargetForInput(const score::gfx::Port& p) override;

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* edge) override;
  void release(score::gfx::RenderList&) override;

  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer& commands, score::gfx::Edge& e) override;

  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override;
};

class SCORE_PLUGIN_GFX_EXPORT BasicRenderer : public score::gfx::OutputNodeRenderer
{
public:
  score::gfx::TextureRenderTarget m_rt;

  score::gfx::TextureRenderTarget renderTargetForInput(const score::gfx::Port& p) override;
  BasicRenderer(score::gfx::TextureRenderTarget outputTarget, const score::gfx::RenderState& state, const score::gfx::Node& parent);
  ~BasicRenderer();

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* edge) override;
  void runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer& commands, score::gfx::Edge& e) override;
  void release(score::gfx::RenderList&) override;
};

}
