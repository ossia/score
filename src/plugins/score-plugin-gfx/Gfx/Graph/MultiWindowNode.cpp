#include <Gfx/Graph/MultiWindowNode.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/OpenGL.hpp>

#include <QGuiApplication>
#include <QScreen>

namespace score::gfx
{

// --- MultiWindowRenderer ---
// Renders the input texture's sub-regions to multiple swap chains.

class MultiWindowRenderer final : public score::gfx::OutputNodeRenderer
{
public:
  // The input target where all upstream nodes render to
  score::gfx::TextureRenderTarget m_inputTarget;

  QShader m_vertexS, m_fragmentS;
  score::gfx::MeshBuffers m_mesh{};

  struct PerWindowData
  {
    score::gfx::Pipeline pipeline;
    QRhiBuffer* uvRectUBO{};
    std::array<score::gfx::Sampler, 1> samplers{};
    QRhiShaderResourceBindings* srb{};
    QRectF sourceRect{0, 0, 1, 1};
  };
  std::vector<PerWindowData> m_perWindow;

  const MultiWindowNode& m_multiNode;

  explicit MultiWindowRenderer(
      const MultiWindowNode& node, const score::gfx::RenderState& state)
      : score::gfx::OutputNodeRenderer{node}
      , m_multiNode{node}
  {
  }

  ~MultiWindowRenderer() override = default;

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return m_inputTarget;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& rhi = *renderer.state.rhi;

    // Create the input target where upstream nodes will render
    m_inputTarget = score::gfx::createRenderTarget(
        renderer.state, QRhiTexture::Format::RGBA8, renderer.state.renderSize,
        renderer.samples(), renderer.requiresDepth(*this->node.input[0]));

    const auto& mesh = renderer.defaultTriangle();
    m_mesh = renderer.initMeshBuffer(mesh, res);

    // Compile the sub-rect sampling shader
    // sourceRect is in canvas space (Y-down: y=0 top, y=1 bottom).
    // Following the ScaledRenderer pattern: on SPIRV flip v_texcoord.y,
    // on OpenGL flip sourceRect.y to convert from canvas Y-down to texture Y-up.
    static const constexpr auto frag_shader = R"_(#version 450
      layout(location = 0) in vec2 v_texcoord;
      layout(location = 0) out vec4 fragColor;

      layout(binding = 3) uniform sampler2D tex;

      layout(std140, binding = 2) uniform SubRect {
          vec4 sourceRect; // x, y, width, height in canvas UV space
      };

      void main()
      {
          vec2 uv;
          uv.x = sourceRect.x + v_texcoord.x * sourceRect.z;
#if defined(QSHADER_SPIRV)
          // SPIRV/Vulkan: flip v_texcoord.y (same as ScaledRenderer),
          // use sourceRect.y directly in canvas space
          uv.y = sourceRect.y + (1.0 - v_texcoord.y) * sourceRect.w;
#else
          // OpenGL: v_texcoord.y is Y-up, convert sourceRect from canvas Y-down to texture Y-up
          float texSrcY = 1.0 - sourceRect.y - sourceRect.w;
          uv.y = texSrcY + v_texcoord.y * sourceRect.w;
#endif
          fragColor = texture(tex, uv);
      }
      )_";

    std::tie(m_vertexS, m_fragmentS)
        = score::gfx::makeShaders(renderer.state, mesh.defaultVertexShader(), frag_shader);

    // Create per-window pipeline data
    const auto& outputs = m_multiNode.windowOutputs();
    m_perWindow.resize(outputs.size());

    for(int i = 0; i < (int)outputs.size(); ++i)
    {
      auto& pw = m_perWindow[i];
      const auto& wo = outputs[i];
      pw.sourceRect = wo.sourceRect;

      // Create UBO for source rect
      pw.uvRectUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 4 * sizeof(float));
      pw.uvRectUBO->setName(QByteArray("MultiWindowRenderer::uvRectUBO_") + QByteArray::number(i));
      pw.uvRectUBO->create();

      // Upload initial source rect values
      float rectData[4] = {
          (float)pw.sourceRect.x(), (float)pw.sourceRect.y(),
          (float)pw.sourceRect.width(), (float)pw.sourceRect.height()};
      res.updateDynamicBuffer(pw.uvRectUBO, 0, sizeof(rectData), rectData);

      // Create sampler pointing to input texture
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->setName("MultiWindowRenderer::sampler");
      sampler->create();

      pw.samplers[0] = {sampler, m_inputTarget.texture};

      // Build pipeline for this window's render pass descriptor
      // The actual render target is fetched at render time from the swap chain
      if(wo.renderPassDescriptor)
      {
        score::gfx::TextureRenderTarget rt;
        rt.renderPass = wo.renderPassDescriptor;

        pw.pipeline = score::gfx::buildPipeline(
            renderer, mesh, m_vertexS, m_fragmentS, rt, nullptr, pw.uvRectUBO,
            pw.samplers);
      }
    }
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }

  void release(score::gfx::RenderList&) override
  {
    for(auto& pw : m_perWindow)
    {
      pw.pipeline.release();
      for(auto& s : pw.samplers)
        delete s.sampler;
      pw.samplers = {};
      delete pw.uvRectUBO;
      pw.uvRectUBO = nullptr;
    }
    m_perWindow.clear();
    m_inputTarget.release();
  }

  // Called during RenderList::render() for the primary window (index 0)
  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
    if(m_perWindow.empty())
      return;

    renderSubRegion(0, renderer, cb, res);
  }

  // Called by MultiWindowNode::render() for secondary windows (index >= 1)
  void renderToWindow(
      int windowIndex, score::gfx::RenderList& renderer, QRhiCommandBuffer& cb)
  {
    if(windowIndex < 0 || windowIndex >= (int)m_perWindow.size())
      return;

    auto* res = renderer.state.rhi->nextResourceUpdateBatch();
    renderSubRegion(windowIndex, renderer, cb, res);
  }

private:
  void renderSubRegion(
      int index, score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res)
  {
    const auto& outputs = m_multiNode.windowOutputs();
    if(index >= (int)outputs.size())
      return;

    const auto& wo = outputs[index];
    auto& pw = m_perWindow[index];

    if(!wo.swapChain || !wo.hasSwapChain)
      return;

    auto rt = wo.swapChain->currentFrameRenderTarget();
    if(!rt)
      return;

    cb.beginPass(rt, Qt::black, {1.0f, 0}, res);
    res = nullptr;
    {
      auto sz = wo.swapChain->currentPixelSize();
      cb.setGraphicsPipeline(pw.pipeline.pipeline);
      cb.setShaderResources(pw.pipeline.srb);
      cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

      const auto& mesh = renderer.defaultTriangle();
      mesh.draw(m_mesh, cb);
    }
    cb.endPass();
  }
};

// --- MultiWindowNode implementation ---

MultiWindowNode::MultiWindowNode(
    Configuration conf, const std::vector<Gfx::OutputMapping>& mappings)
    : OutputNode{}
    , m_conf{conf}
    , m_mappings{mappings}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
}

MultiWindowNode::~MultiWindowNode()
{
  for(auto& wo : m_windowOutputs)
  {
    if(wo.swapChain)
    {
      wo.swapChain->deleteLater();
      wo.swapChain = nullptr;
    }
    delete wo.depthStencil;
    wo.depthStencil = nullptr;
    delete wo.renderPassDescriptor;
    wo.renderPassDescriptor = nullptr;
  }

  if(m_renderState)
  {
    // Window 0's RPD was already deleted in the loop above
    m_renderState->renderPassDescriptor = nullptr;
    delete m_renderState->rhi;
    m_renderState->rhi = nullptr;
    delete m_renderState->surface;
    m_renderState->surface = nullptr;
  }
}

bool MultiWindowNode::canRender() const
{
  return !m_windowOutputs.empty() && m_renderState && m_renderState->rhi;
}

void MultiWindowNode::startRendering()
{
  if(onFps)
    onFps(0.f);
}

void MultiWindowNode::render()
{
  auto rl = m_renderer.lock();
  if(!rl || rl->renderers.size() <= 1)
    return;

  auto rhi = m_renderState->rhi;

  // Handle swap chain resizes for all windows
  for(auto& wo : m_windowOutputs)
  {
    if(!wo.window || !wo.swapChain)
      continue;
    if(wo.swapChain->currentPixelSize() != wo.swapChain->surfacePixelSize())
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
    }
  }

  // Phase 1: Render upstream graph + blit sub-region 0 via the primary window
  if(m_windowOutputs.empty())
    return;

  auto& primaryWO = m_windowOutputs[0];
  if(!primaryWO.window || !primaryWO.swapChain || !primaryWO.hasSwapChain)
    return;

  QRhi::FrameOpResult r = rhi->beginFrame(primaryWO.swapChain);
  if(r == QRhi::FrameOpSwapChainOutOfDate)
  {
    primaryWO.hasSwapChain = primaryWO.swapChain->createOrResize();
    if(!primaryWO.hasSwapChain)
      return;
    r = rhi->beginFrame(primaryWO.swapChain);
  }
  if(r != QRhi::FrameOpSuccess)
    return;

  auto cb = primaryWO.swapChain->currentFrameCommandBuffer();
  rl->render(*cb);

  rhi->endFrame(primaryWO.swapChain);

  // Phase 2: For each additional window, blit the sub-region
  if(this->renderedNodes.empty())
    return;
  auto outRenderer = dynamic_cast<MultiWindowRenderer*>(
      this->renderedNodes.begin()->second);
  if(!outRenderer)
    return;

  for(int i = 1; i < (int)m_windowOutputs.size(); ++i)
  {
    auto& wo = m_windowOutputs[i];
    if(!wo.window || !wo.swapChain || !wo.hasSwapChain)
      continue;

    r = rhi->beginFrame(wo.swapChain);
    if(r == QRhi::FrameOpSwapChainOutOfDate)
    {
      wo.hasSwapChain = wo.swapChain->createOrResize();
      if(!wo.hasSwapChain)
        continue;
      r = rhi->beginFrame(wo.swapChain);
    }
    if(r != QRhi::FrameOpSuccess)
      continue;

    cb = wo.swapChain->currentFrameCommandBuffer();
    outRenderer->renderToWindow(i, *rl, *cb);

    rhi->endFrame(wo.swapChain);
  }
}

void MultiWindowNode::onRendererChange()
{
}

void MultiWindowNode::stopRendering()
{
  if(onFps)
    onFps(0.f);
}

void MultiWindowNode::setRenderer(std::shared_ptr<RenderList> r)
{
  m_renderer = r;
}

RenderList* MultiWindowNode::renderer() const
{
  return m_renderer.lock().get();
}

void MultiWindowNode::initWindow(int index, GraphicsApi api)
{
  auto& wo = m_windowOutputs[index];
  auto& mapping = m_mappings[index];

  auto rhi = m_renderState->rhi;

  // Create swap chain
  wo.swapChain = rhi->newSwapChain();
  wo.swapChain->setWindow(wo.window.get());

  wo.depthStencil = rhi->newRenderBuffer(
      QRhiRenderBuffer::DepthStencil, QSize(),
      m_renderState->samples, QRhiRenderBuffer::UsedWithSwapChainOnly);
  wo.swapChain->setDepthStencil(wo.depthStencil);
  wo.swapChain->setSampleCount(m_renderState->samples);

  QRhiSwapChain::Flags flags = QRhiSwapChain::MinimalBufferCount;
  // Only first window may use VSync; others use NoVSync
  if(index > 0)
    flags |= QRhiSwapChain::NoVSync;
  else if(!score::AppContext().settings<Gfx::Settings::Model>().getVSync())
    flags |= QRhiSwapChain::NoVSync;
  wo.swapChain->setFlags(flags);

  wo.renderPassDescriptor = wo.swapChain->newCompatibleRenderPassDescriptor();
  wo.swapChain->setRenderPassDescriptor(wo.renderPassDescriptor);

  // Store the first window's RPD in the render state
  if(index == 0)
    m_renderState->renderPassDescriptor = wo.renderPassDescriptor;

  wo.sourceRect = mapping.sourceRect;
}

void MultiWindowNode::createOutput(score::gfx::OutputConfiguration conf)
{
  if(m_mappings.empty())
    return;

  // Create shared QRhi without a specific window
  m_renderState = score::gfx::createRenderState(conf.graphicsApi, QSize{1280, 720}, nullptr);
  if(!m_renderState || !m_renderState->rhi)
    return;

  m_windowOutputs.resize(m_mappings.size());

  // Create all windows
  for(int i = 0; i < (int)m_mappings.size(); ++i)
  {
    auto& wo = m_windowOutputs[i];
    auto& mapping = m_mappings[i];

    wo.window = std::make_shared<Window>(conf.graphicsApi);
    wo.window->setTitle(
        QString("Output %1").arg(i));

    if(mapping.fullscreen)
    {
      // Set screen if specified
      if(mapping.screenIndex >= 0)
      {
        const auto& screens = qApp->screens();
        if(mapping.screenIndex < screens.size())
          wo.window->setScreen(screens[mapping.screenIndex]);
      }
      wo.window->showFullScreen();
    }
    else
    {
      wo.window->resize(mapping.windowSize);
      wo.window->setPosition(mapping.windowPosition);

      if(mapping.screenIndex >= 0)
      {
        const auto& screens = qApp->screens();
        if(mapping.screenIndex < screens.size())
          wo.window->setScreen(screens[mapping.screenIndex]);
      }

      wo.window->show();
    }
  }

  // Set up a callback to initialize swap chains once first window is exposed
  // For simplicity, we use a timer-based approach to wait for windows to be exposed
  // Then init all swap chains
  auto initAll = [this, onReady = std::move(conf.onReady)]() mutable {
    for(int i = 0; i < (int)m_windowOutputs.size(); ++i)
    {
      initWindow(i, m_renderState->api);
      auto& wo = m_windowOutputs[i];
      wo.hasSwapChain = wo.swapChain->createOrResize();
      if(wo.hasSwapChain && wo.window)
      {
        m_renderState->outputSize = wo.swapChain->currentPixelSize();
      }
    }

    if(onReady)
      onReady();
  };

  // The first window's onWindowReady triggers initialization
  m_windowOutputs[0].window->onWindowReady = std::move(initAll);

  // Wire resize for all windows
  for(int i = 0; i < (int)m_windowOutputs.size(); ++i)
  {
    m_windowOutputs[i].window->onResize = [this, i, onResize = conf.onResize] {
      if(i < (int)m_windowOutputs.size())
      {
        auto& wo = m_windowOutputs[i];
        if(wo.swapChain)
          wo.hasSwapChain = wo.swapChain->createOrResize();
      }
      // Only trigger pipeline rebuild from first window resize
      if(i == 0 && onResize)
        onResize();
    };
  }
}

void MultiWindowNode::destroyOutput()
{
  for(auto& wo : m_windowOutputs)
  {
    delete wo.depthStencil;
    wo.depthStencil = nullptr;

    // Don't delete RPD for index 0 as it's owned by m_renderState
    // Actually, we manage them all individually
    if(wo.renderPassDescriptor && wo.renderPassDescriptor != m_renderState->renderPassDescriptor)
    {
      delete wo.renderPassDescriptor;
    }
    wo.renderPassDescriptor = nullptr;

    delete wo.swapChain;
    wo.swapChain = nullptr;

    wo.window.reset();
  }
  m_windowOutputs.clear();

  if(m_renderState)
  {
    delete m_renderState->renderPassDescriptor;
    m_renderState->renderPassDescriptor = nullptr;
    m_renderState->destroy();
  }
}

void MultiWindowNode::updateGraphicsAPI(GraphicsApi api)
{
  if(!m_renderState)
    return;

  if(m_renderState->api != api)
    destroyOutput();
}

void MultiWindowNode::setVSyncCallback(std::function<void()> f)
{
  m_vsyncCallback = std::move(f);
}

std::shared_ptr<RenderState> MultiWindowNode::renderState() const
{
  return m_renderState;
}

OutputNodeRenderer* MultiWindowNode::createRenderer(RenderList& r) const noexcept
{
  return new MultiWindowRenderer{*this, r.state};
}

OutputNode::Configuration MultiWindowNode::configuration() const noexcept
{
  return m_conf;
}

}
