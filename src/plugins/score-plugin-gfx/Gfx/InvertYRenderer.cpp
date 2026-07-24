#include "InvertYRenderer.hpp"

#include <Gfx/Graph/RenderList.hpp>

namespace Gfx
{

InvertYRenderer::InvertYRenderer(
    const score::gfx::Node& n, score::gfx::TextureRenderTarget rt,
    QRhiReadbackResult& readback)
    : score::gfx::OutputNodeRenderer{n}
    , m_inputTarget{std::move(rt)}
    , m_readback{&readback}
{
}

void InvertYRenderer::init(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  // Re-adopt the owning output node's CURRENT render target + render-pass
  // descriptor. The sink may have recreated them since this renderer was
  // constructed: a BackgroundNode viewport resize destroys the old
  // QRhiTextureRenderTarget / QRhiRenderPassDescriptor (deleteLater) and
  // installs fresh ones. When the resize takes the in-place fast path
  // (RenderList::resizeSwapchainSizedTargets -> maybeRebuild's
  // release()+init()) this renderer is NOT reconstructed, so the cached
  // m_inputTarget would still reference the freed target/renderpass — and
  // the upstream node's final pass (RenderedISFNode::addOutputPass ->
  // renderTargetForOutput -> renderTargetForInput) would build its pipeline
  // against a stale VkRenderPass. That is a Vulkan use-after-free: the
  // driver dereferences the destroyed VkRenderPass in vkCreateGraphicsPipelines
  // (validation reports VK_ERROR_VALIDATION_FAILED_EXT / -1000011001, and the
  // NVIDIA driver may SIGSEGV outright). Refreshing here — before the upstream
  // renderers are re-init'd in the same maybeRebuild pass (the output renderer
  // is first in RenderList::renderers) — rebinds the live handles.
  if(auto* out = dynamic_cast<const score::gfx::OutputNode*>(&this->node))
  {
    auto cur = out->currentRenderTarget();
    if(cur.renderTarget && cur.renderPass)
      m_inputTarget = cur;
  }

  m_renderTarget = score::gfx::createRenderTarget(
      renderer.state, renderer.state.renderFormat, m_inputTarget.texture->pixelSize(),
      renderer.samples(), renderer.requiresDepth(*this->node.input[0]));

  const auto& mesh = renderer.defaultTriangle();
  m_mesh = renderer.initMeshBuffer(mesh, res);

  // We need to have a pass to invert the Y coordinate to go
  // from GL direction (Y up) to normal video (Y down)
  // FIXME spout likely needs the same
  static const constexpr auto gl_filter = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    layout(binding = 3) uniform sampler2D tex;

    void main()
    {
#if defined(QSHADER_MSL) || defined(QSHADER_HLSL)
      // Metal already has attachment output inverted
      fragColor = texture(tex, vec2(v_texcoord.x, v_texcoord.y));
#else
      fragColor = texture(tex, vec2(v_texcoord.x, 1. - v_texcoord.y));
#endif
    }
    )_";
  std::tie(m_vertexS, m_fragmentS)
      = score::gfx::makeShaders(renderer.state, mesh.defaultVertexShader(), gl_filter);

  // Put the input texture, where all the input nodes are rendering, in a sampler.
  {
    auto sampler = renderer.state.rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

    sampler->setName("FullScreenImageNode::sampler");
    sampler->create();

    m_samplers.push_back({sampler, this->m_inputTarget.texture});
  }

  m_p = score::gfx::buildPipeline(
      renderer, mesh, m_vertexS, m_fragmentS, m_renderTarget, nullptr, nullptr,
      m_samplers);
}

void InvertYRenderer::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* e)
{
}

void InvertYRenderer::release(score::gfx::RenderList&)
{
  m_p.release();
  for(auto& s : m_samplers)
  {
    delete s.sampler;
  }
  m_samplers.clear();
  m_renderTarget.release();
}

void InvertYRenderer::finishFrame(
    score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
    QRhiResourceUpdateBatch*& res)
{
  cb.beginPass(m_renderTarget.renderTarget, Qt::black, {0.0f, 0}, res);
  res = nullptr;
  // m_p.pipeline is null when buildPipeline's QRhiGraphicsPipeline::create()
  // failed (transient during graph rebuild). setGraphicsPipeline asserts on a
  // null pipeline (Q_ASSERT) and dereferences it in release builds, so skip
  // the draw — the target is still cleared and read back (as black).
  if(m_p.pipeline)
  {
    const auto sz = renderer.state.renderSize;
    cb.setGraphicsPipeline(m_p.pipeline);
    cb.setShaderResources(m_p.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    const auto& mesh = renderer.defaultTriangle();
    mesh.draw(this->m_mesh, cb);
  }

  auto next = renderer.state.rhi->nextResourceUpdateBatch();

  QRhiReadbackDescription rb(m_renderTarget.texture);
  next->readBackTexture(rb, m_readback);
  cb.endPass(next);

  next = nullptr;
}



ScaledRenderer::ScaledRenderer(score::gfx::TextureRenderTarget outputTarget, const score::gfx::RenderState &state, const score::gfx::Node &parent)
    : score::gfx::OutputNodeRenderer{parent}
    , m_renderTarget{outputTarget}
{
}

ScaledRenderer::~ScaledRenderer() { }

void ScaledRenderer::init(score::gfx::RenderList &renderer, QRhiResourceUpdateBatch &res)
{
  m_inputTarget = score::gfx::createRenderTarget(
      renderer.state, renderer.state.renderFormat, renderer.state.renderSize,
      renderer.samples(), renderer.requiresDepth(*this->node.input[0]));

  const auto& mesh = renderer.defaultTriangle();
  m_mesh = renderer.initMeshBuffer(mesh, res);
  static const constexpr auto gl_filter = R"_(#version 450
      layout(location = 0) in vec2 v_texcoord;
      layout(location = 0) out vec4 fragColor;

      layout(binding = 3) uniform sampler2D tex;

      void main()
      {
#if defined(QSHADER_SPIRV)
        fragColor = texture(tex, vec2(v_texcoord.x, 1. - v_texcoord.y));
#else
        fragColor = texture(tex, vec2(v_texcoord.x, v_texcoord.y));
#endif
      }
      )_";

  std::tie(m_vertexS, m_fragmentS)
      = score::gfx::makeShaders(renderer.state, mesh.defaultVertexShader(), gl_filter);

  m_samplers = {};
  // Put the input texture, where all the input nodes are rendering, in a sampler.
  {
    auto sampler = renderer.state.rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

    sampler->setName("FullScreenImageNode::sampler");
    sampler->create();

    m_samplers[0] = {sampler, this->m_inputTarget.texture};
  }

  m_p = score::gfx::buildPipeline(
      renderer, mesh, m_vertexS, m_fragmentS, m_renderTarget, nullptr, nullptr,
      m_samplers);
}

void ScaledRenderer::update(score::gfx::RenderList &renderer, QRhiResourceUpdateBatch &res, score::gfx::Edge *edge) {
}

void ScaledRenderer::runRenderPass(score::gfx::RenderList &, QRhiCommandBuffer &commands, score::gfx::Edge &e)
{
  // m_rt.renderTarget = parent.m_swapChain->currentFrameRenderTarget();
  // m_rt.renderPass = state->renderPassDescriptor;
}

void ScaledRenderer::finishFrame(score::gfx::RenderList &renderer, QRhiCommandBuffer &cb, QRhiResourceUpdateBatch *&res)
{
  cb.beginPass(m_renderTarget.renderTarget, Qt::black, {0.0f, 0}, res);
  res = nullptr;
  // See InvertYRenderer::finishFrame: skip the draw if the pipeline failed to
  // build (null), rather than asserting/dereferencing in setGraphicsPipeline.
  if(m_p.pipeline)
  {
    const auto sz = renderer.state.outputSize;

    cb.setGraphicsPipeline(m_p.pipeline);
    cb.setShaderResources(m_p.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    const auto& mesh = renderer.defaultTriangle();
    mesh.draw(this->m_mesh, cb);
  }
  cb.endPass();
}

void ScaledRenderer::release(score::gfx::RenderList &)
{
  m_p.release();
  m_inputTarget.release();
  for(auto& s : m_samplers)
  {
    delete s.sampler;
  }
  m_samplers = {};
  //m_renderTarget.release();
}

score::gfx::TextureRenderTarget ScaledRenderer::renderTargetForInput(const score::gfx::Port &p)
{
  return m_inputTarget;
}


score::gfx::TextureRenderTarget BasicRenderer::renderTargetForInput(const score::gfx::Port &p) { return m_rt; }

BasicRenderer::BasicRenderer(score::gfx::TextureRenderTarget outputTarget, const score::gfx::RenderState &state, const score::gfx::Node &parent)
    : score::gfx::OutputNodeRenderer{parent}
    , m_rt{outputTarget}
{
}

BasicRenderer::~BasicRenderer() { }

void BasicRenderer::init(score::gfx::RenderList &renderer, QRhiResourceUpdateBatch &res) { }

void BasicRenderer::update(score::gfx::RenderList &renderer, QRhiResourceUpdateBatch &res, score::gfx::Edge *edge) {
}

void BasicRenderer::runRenderPass(score::gfx::RenderList &, QRhiCommandBuffer &commands, score::gfx::Edge &e) { }

void BasicRenderer::release(score::gfx::RenderList &) { }

}
