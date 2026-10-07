#include "PreviewNode.hpp"

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/PipelineStateHelpers.hpp>
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

  state.caps.populate(*rhi);
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
  if(m_deviceLost)
    return;
  auto renderer = m_renderer.lock();
  if(renderer && m_renderState)
  {
    auto rhi = m_renderState->rhi;
    score::gfx::OffscreenFrame frame{*rhi};
    if(!frame)
    {
      checkDeviceLost(frame, *rhi, "PreviewNode");
      return;
    }

    renderer->render(frame.commands());
    frame.end();
    checkDeviceLost(frame, *rhi, "PreviewNode");
  }
}

score::gfx::OutputNode::Configuration PreviewNode::configuration() const noexcept
{
  return {.hostDriven = true};
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
  resetDeviceLost();
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

// Renders the graph into an intermediate texture and blits that into the host
// texture, which turns the picture over on the way.
//
// WHERE THE FLIP COMES FROM, per backend. The blit uses the default full-screen
// triangle, whose vertex shader is `gl_Position = clipSpaceCorrMatrix *
// vec4(position, 0, 1)` with `v_texcoord = texcoord` and `texcoord.y =
// (position.y + 1) / 2`. Pair that with where NDC -Y lands in memory and the
// sampling below is an identity in texture coordinates but a Y flip in memory
// on every backend except OpenGL:
//
//     backend | clipSpaceCorrMatrix Y | NDC Y | first memory row is at
//     OpenGL  |         +1            |  up   | y_clip = -1  ->  v = 0  (identity)
//     Vulkan  |         -1            | down  | y_clip = -1  ->  v = 1  (flip)
//     Metal   |         +1            |  up   | y_clip = +1  ->  v = 1  (flip)
//     D3D     |         +1            |  up   | y_clip = +1  ->  v = 1  (flip)
//
// So the fragment shader samples v straight: the flip is in the geometry, not in
// the sampling, and writing `1. - v` here for Vulkan would cancel it exactly --
// which is measurably indistinguishable from not using this renderer at all.
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
        renderer.samples(),
        renderer.requiresDepth(*this->node.input[0])
            || renderer.anyNodeRequiresDepth());

    const auto& mesh = renderer.defaultTriangle();
    m_mesh = renderer.initMeshBuffer(mesh, res);

    // Identity sampling, deliberately: the per-backend table above this class
    // explains why that is already a memory-space Y flip here, and why a
    // `1. - v` under QSHADER_SPIRV would undo precisely the inversion it looks
    // like it is adding. Unlike Gfx/InvertYRenderer.cpp, which targets a
    // top-down video buffer and so has to spell the flip out in the shader.
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
        m_samplers, score::gfx::premultipliedOverBlend());
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

bool previewFirstRowIsPictureBottom(QRhi& rhi) noexcept
{
  // Must stay in step with createRenderer below: true exactly for the backends
  // whose host texture ends up in OpenGL's row order.
  switch(rhi.backend())
  {
    case QRhi::OpenGLES2:
      // Pass-through, and OpenGL's framebuffer origin is already at the bottom.
      return true;
    case QRhi::Vulkan:
      // Pass-through would leave the picture's top at the first row; the blit
      // puts it back the OpenGL way.
      return true;
    default:
      // Metal and D3D keep the renderer they have always had. Whether the
      // result is OpenGL's order there is NOT established -- see createRenderer.
      return false;
  }
}

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
      // Nothing to correct: the graph's last pass draws straight into the
      // item's render target, and OpenGL's framebuffer order already puts the
      // picture's bottom in the texture's first row.
      return new score::gfx::PreviewRenderer{*this, rt};

    case score::gfx::GraphicsApi::Vulkan:
      // Vulkan is the one backend where clipSpaceCorrMatrix negates Y, so a
      // shader's output position lands in memory the other way up than it does
      // on OpenGL. Every other sink in score has a stage that absorbs that:
      // InvertYRenderer spells out `1. - v` for a readback target, and
      // ScaledRenderer does the same for a swapchain ("only Vulkan needs the
      // correction, because only there did clipSpaceCorrMatrix already flip
      // clip space"). A preview had none, so it handed Qt Quick 3D a texture
      // mirrored against the one OpenGL hands it -- for a fisheye source, an
      // azimuth mirror in the projected dome. The blit is that missing stage.
      return new score::gfx::PreviewRendererInvertY{*this, rt};

    case score::gfx::GraphicsApi::Metal:
    case score::gfx::GraphicsApi::D3D11:
    case score::gfx::GraphicsApi::D3D12:
      // UNCHANGED, and not because it is known to be right. Neither backend can
      // be created on Linux, so there is no measurement of what row order they
      // end up with; the table above PreviewRendererInvertY predicts that this
      // blit also flips there, which -- combined with score's ISF vertex stage
      // negating Y for MSL and HLSL as well -- would leave Metal and D3D in the
      // backend's own order rather than OpenGL's, i.e. still wrong for the
      // Quick3D consumer and right for the 2D one. Settling that needs a run of
      // tests/gfx/GfxPreviewSourceItemOrientation.cpp on Apple or Windows
      // hardware, and until then this change does not touch them: the 2D
      // preview there keeps working exactly as it does today.
      return new score::gfx::PreviewRendererInvertY{*this, rt};
  }
  return nullptr;
}

}
