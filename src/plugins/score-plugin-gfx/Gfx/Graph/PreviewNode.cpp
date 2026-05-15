#include "PreviewNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/gfx/OpenGL.hpp>
namespace score::gfx
{
std::shared_ptr<RenderState> importRenderState(QSize sz, QRhi* rhi)
{
  auto st = std::make_shared<RenderState>();
  RenderState& state = *st;
  switch(rhi->backend())
  {
    case QRhi::OpenGLES2:
      state.api = score::gfx::OpenGL;
      break;
    case QRhi::Vulkan:
      state.api = score::gfx::Vulkan;
      break;
    case QRhi::Metal:
      state.api = score::gfx::Metal;
      break;
    case QRhi::D3D11:
      state.api = score::gfx::D3D11;
      break;
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhi::D3D12:
      state.api = score::gfx::D3D12;
      break;
#endif
    case QRhi::Null:
      state.api = score::gfx::Null;
      break;
  }
  state.version = Gfx::Settings::shaderVersionForAPI(state.api);
  state.rhi = rhi;
  // The host widget owns this rhi, so we can't follow the global samples
  // setting here — but we should at least query what the rhi actually
  // supports rather than assuming 1. Final RT sample count is set by the
  // host via setSampleCount on its own swap chain.
  state.samples = rhi->supportedSampleCounts().value(0, 1);
  state.renderSize = sz;
  state.outputSize = sz;

  // Populate the same caps probe ScreenNode/Background/MultiWindow do via
  // createRenderState(), so feature gating in shaders / renderers behaves
  // identically when running inside a preview widget. The rhi is borrowed
  // (host-owned), so we don't install preRhiDestroy / savePipelineCache —
  // those are the host's responsibility.
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
  state.caps.drawIndirect = rhi->isFeatureSupported(QRhi::DrawIndirect);
  state.caps.drawIndirectMulti = rhi->isFeatureSupported(QRhi::DrawIndirectMulti);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
  state.caps.instanceIndexIncludesBaseInstance
      = rhi->isFeatureSupported(QRhi::InstanceIndexIncludesBaseInstance);
  state.caps.depthClamp = rhi->isFeatureSupported(QRhi::DepthClamp);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  state.caps.variableRateShading = rhi->isFeatureSupported(QRhi::VariableRateShading);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  state.caps.textureViewFormat = rhi->isFeatureSupported(QRhi::TextureViewFormat);
  state.caps.resolveDepthStencil = rhi->isFeatureSupported(QRhi::ResolveDepthStencil);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  state.caps.multiview = rhi->isFeatureSupported(QRhi::MultiView);
#endif

  state.caps.timestamps = rhi->isFeatureSupported(QRhi::Timestamps);
  state.caps.tessellation = rhi->isFeatureSupported(QRhi::Tessellation);
  state.caps.geometryShader = rhi->isFeatureSupported(QRhi::GeometryShader);
  state.caps.baseInstance = rhi->isFeatureSupported(QRhi::BaseInstance);
  state.caps.pipelineCacheDataLoadSave
      = rhi->isFeatureSupported(QRhi::PipelineCacheDataLoadSave);

  return st;
}

PreviewNode::PreviewNode(
    Gfx::SharedOutputSettings s, QRhi* rhi, QRhiRenderTarget* tgt, QRhiTexture* tex)
    : OutputNode{}
    , m_settings{std::move(s)}
    , m_rhi{rhi}
    , m_renderTarget{tgt}
    , m_texture{tex}
{
  input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
}

PreviewNode::~PreviewNode() { }

bool PreviewNode::canRender() const
{
  return true;
}

void PreviewNode::startRendering() { }

void PreviewNode::render()
{
  auto renderer = m_renderer.lock();
  if(renderer && m_renderState)
  {
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;

    renderer->render(*cb);
    rhi->endOffscreenFrame();
  }
}

score::gfx::OutputNode::Configuration PreviewNode::configuration() const noexcept
{
  return {};
}

void PreviewNode::onRendererChange() { }

void PreviewNode::stopRendering() { }

void PreviewNode::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{
  m_renderer = r;
}

score::gfx::RenderList* PreviewNode::renderer() const
{
  return m_renderer.lock().get();
}

void PreviewNode::createOutput(score::gfx::OutputConfiguration conf)
{
  m_renderState = std::make_shared<score::gfx::RenderState>();

  m_renderState = importRenderState(QSize(m_settings.width, m_settings.height), m_rhi);
  m_renderState->renderPassDescriptor = m_renderTarget->renderPassDescriptor();

  conf.onReady();
}

void PreviewNode::destroyOutput()
{
  // Persist-across-rebuild contract: registry survives RL teardown,
  // so its QRhi resources must be released here (BEFORE we drop our
  // RenderState reference) while the host-owned QRhi is still alive.
  // The host (Qt widget) is responsible for outliving us, but we tear
  // down our own resources first to keep the contract symmetric with
  // ScreenNode / BackgroundNode / MultiWindowNode.
  releaseRegistry();

  // Host owns the underlying QRhi and the m_renderTarget / m_texture aliases
  // — we don't free those. The shared_ptr<RenderState> is the only piece
  // PreviewNode actually owns; reset it so a createOutput → destroyOutput →
  // createOutput cycle drops the prior state instead of relying on
  // make_shared assignment to release the previous holder. Matches the
  // unified sink contract every other OutputNode subclass observes.
  m_renderState.reset();
}

std::shared_ptr<score::gfx::RenderState> PreviewNode::renderState() const
{
  return m_renderState;
}

class PreviewRenderer final : public score::gfx::OutputNodeRenderer
{
public:
  explicit PreviewRenderer(const score::gfx::Node& n, score::gfx::TextureRenderTarget rt)
      : score::gfx::OutputNodeRenderer{n}
      , m_inputTarget{std::move(rt)}
  {
  }

  score::gfx::TextureRenderTarget m_inputTarget;
  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return m_inputTarget;
  }

  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override { }
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }
  void release(score::gfx::RenderList&) override { }
};

class PreviewRendererInvertY final : public score::gfx::OutputNodeRenderer
{
  score::gfx::TextureRenderTarget m_inputTarget;
  score::gfx::TextureRenderTarget m_renderTarget;

  QShader m_vertexS, m_fragmentS;

  std::vector<score::gfx::Sampler> m_samplers;

  score::gfx::Pipeline m_p;

  score::gfx::MeshBuffers m_mesh{};

public:
  explicit PreviewRendererInvertY(
      const score::gfx::Node& n, score::gfx::TextureRenderTarget rt)
      : score::gfx::OutputNodeRenderer{n}
      , m_renderTarget{std::move(rt)}
  {
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return m_inputTarget;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    m_inputTarget = score::gfx::createRenderTarget(
        renderer.state, renderer.state.renderFormat, m_renderTarget.texture->pixelSize(),
        renderer.samples(), renderer.requiresDepth(*this->node.input[0]));

    const auto& mesh = renderer.defaultTriangle();
    m_mesh = renderer.initMeshBuffer(mesh, res);

    // Note that unlike the "InvertYRenderer", here we leverage the
    // coordinate inversion inherent to metal rendering (e.g. y direction is
    // different in viewport vs texture, thus displaying a texture "inverts" it)
    static const constexpr auto gl_filter = R"_(#version 450
    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    layout(binding = 3) uniform sampler2D tex;

    void main()
    {
      fragColor = texture(tex, vec2(v_texcoord.x,  v_texcoord.y));
    }
    )_";
    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, mesh.defaultVertexShader(), gl_filter);

    // Put the input texture, where all the input nodes are rendering, in a sampler.
    {
      auto sampler = renderer.state.rhi->newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

      sampler->setName("PreviewRendererInvertY::sampler");
      sampler->create();

      m_samplers.push_back({sampler, this->m_inputTarget.texture});
    }

    m_p = score::gfx::buildPipeline(
        renderer, mesh, m_vertexS, m_fragmentS, m_renderTarget, nullptr, nullptr,
        m_samplers);
  }
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }
  void release(score::gfx::RenderList&) override
  {
    m_p.release();
    for(auto& s : m_samplers)
    {
      delete s.sampler;
    }
    m_samplers.clear();
    m_inputTarget.release();
  }

  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
    cb.beginPass(m_renderTarget.renderTarget, Qt::black, {0.0f, 0}, res);
    res = nullptr;
    {
      const auto sz = renderer.state.renderSize;
      cb.setGraphicsPipeline(m_p.pipeline);
      cb.setShaderResources(m_p.srb);
      cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

      const auto& mesh = renderer.defaultTriangle();
      mesh.draw(this->m_mesh, cb);
    }
    cb.endPass(nullptr);
  }
};

score::gfx::OutputNodeRenderer*
PreviewNode::createRenderer(score::gfx::RenderList& r) const noexcept
{
  score::gfx::TextureRenderTarget rt{
      .texture = m_texture,
      .renderPass = m_renderState->renderPassDescriptor,
      .renderTarget = m_renderTarget};
  switch(r.state.api)
  {
    default:
    case score::gfx::GraphicsApi::OpenGL:
    case score::gfx::GraphicsApi::Vulkan:
      return new score::gfx::PreviewRenderer{*this, rt};
      break;
    case score::gfx::GraphicsApi::Metal:
    case score::gfx::GraphicsApi::D3D11:
    case score::gfx::GraphicsApi::D3D12:
      return new score::gfx::PreviewRendererInvertY{*this, rt};
      break;
  }
  return nullptr;
}

}
