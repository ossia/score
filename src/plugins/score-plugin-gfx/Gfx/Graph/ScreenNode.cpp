#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/ScreenNode.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/gfx/OpenGL.hpp>

#include <QtGui/private/qrhinull_p.h>

#ifndef QT_NO_OPENGL
#include <Gfx/Settings/Model.hpp>

#include <QOffscreenSurface>
#include <QtGui/private/qrhigles2_p.h>
#endif

#include <score/gfx/Vulkan.hpp>
#if QT_HAS_VULKAN
#if __has_include(<QtGui/private/qrhivulkan_p.h>)
#include <QtGui/private/qrhivulkan_p.h>
#else
#undef QT_HAS_VULKAN
#endif
#endif

#ifdef Q_OS_WIN
#include <QtGui/private/qrhid3d11_p.h>
#endif

#ifdef Q_OS_DARWIN
#include <QtGui/private/qrhimetal_p.h>
#endif

#include <QOffscreenSurface>
#include <QScreen>
#include <QWindow>

namespace score::gfx
{
static std::shared_ptr<RenderState>
createRenderState(QWindow& window, GraphicsApi graphicsApi)
{
  qDebug("createRenderState?");
  auto st = std::make_shared<RenderState>();
  RenderState& state = *st;
  state.api = graphicsApi;

  const auto& settings = score::AppContext().settings<Gfx::Settings::Model>();
  state.samples = settings.getSamples();

  QRhi::Flags flags{};
#ifndef NDEBUG
  flags |= QRhi::EnableDebugMarkers;
#endif

#ifndef QT_NO_OPENGL
  if(graphicsApi == OpenGL)
  {
    state.surface = QRhiGles2InitParams::newFallbackSurface();
    QRhiGles2InitParams params;
    params.format = window.format();
    params.fallbackSurface = state.surface;
    params.window = &window;

    score::GLCapabilities caps;
    caps.setupFormat(params.format);
    params.format.setSamples(state.samples);
    state.version = caps.qShaderVersion;
    state.rhi = QRhi::create(QRhi::OpenGLES2, &params, flags);
    state.renderSize = window.size();
    return st;
  }
#endif

#if QT_HAS_VULKAN
  if(graphicsApi == Vulkan)
  {
    QRhiVulkanInitParams params;
    params.inst = window.vulkanInstance();
    params.window = &window;
    // Note: QShaderVersion still hardcoded to 100 in qrhvulkan.cpp as of qt 6.9
    state.version = QShaderVersion(100);
    state.rhi = QRhi::create(QRhi::Vulkan, &params, flags);
    state.renderSize = window.size();
    return st;
  }
#endif

#ifdef Q_OS_WIN
  if(graphicsApi == D3D11)
  {
    QRhiD3D11InitParams params;
#if !defined(NDEBUG)
    params.enableDebugLayer = true;
#endif
    // if (framesUntilTdr > 0)
    // {
    //   params.framesUntilKillingDeviceViaTdr = framesUntilTdr;
    //   params.repeatDeviceKill = true;
    // }
    state.version = QShaderVersion(50);
    state.rhi = QRhi::create(QRhi::D3D11, &params, flags);
    state.renderSize = window.size();
    return st;
  }
#endif

#ifdef Q_OS_DARWIN
  if(graphicsApi == Metal)
  {
    QRhiMetalInitParams params;
    state.version = QShaderVersion(12);
    state.rhi = QRhi::create(QRhi::Metal, &params, flags);
    state.renderSize = window.size();
    return st;
  }
#endif

  if(!state.rhi)
  {
    qDebug() << "Failed to create RHI backend, creating Null backend";

    QRhiNullInitParams params;
    state.version = QShaderVersion(120);
    state.rhi = QRhi::create(QRhi::Null, &params, flags);
    state.renderSize = window.size();
    state.api = GraphicsApi::Null;
    return st;
  }

  return st;
}

ScreenNode::ScreenNode(bool embedded, bool fullScreen)
    : OutputNode{}
    , m_embedded{embedded}
    , m_fullScreen{fullScreen}
    , m_ownsWindow{true}
{
  input.push_back(new Port{this, {}, Types::Image, {}});
}

ScreenNode::~ScreenNode()
{
  if(m_swapChain)
  {
    m_swapChain->deleteLater();

    if(m_window)
    {
      m_window->m_swapChain = nullptr;
      m_window->m_hasSwapChain = false;
    }
  }

  if(m_window && m_window->state)
  {
    delete m_window->state->renderPassDescriptor;
    m_window->state->renderPassDescriptor = nullptr;

    delete m_depthStencil;
    m_depthStencil = nullptr;

    delete m_window->state->rhi;
    m_window->state->rhi = nullptr;

    delete m_window->state->surface;
    m_window->state->surface = nullptr;
  }
}

bool ScreenNode::canRender() const
{
  // FIXME - Graph::onReady / Graph::onResize :
  // swapchain is recreated even when size does not change at the
  // beginning
  return bool(m_window) /* && m_window->m_hasSwapChain */;
}

void ScreenNode::startRendering()
{
  if(m_window)
  {
    m_window->onRender = [this](QRhiCommandBuffer& commands) {
      if(auto r = m_window->state->renderer.lock())
      {
        m_window->m_canRender = r->renderers.size() > 1;
        r->render(commands);
      }
    };
  }
}

void ScreenNode::render() { }

void ScreenNode::onRendererChange()
{
  if(m_window)
  {
    if(auto r = m_window->state->renderer.lock())
    {
      m_window->m_canRender = r->renderers.size() > 1;
    }
    else
    {
      m_window->m_canRender = false;
    }
  }
}

void ScreenNode::stopRendering()
{
  if(m_window)
  {
    m_window->m_canRender = false;
    m_window->onRender = [](QRhiCommandBuffer&) {};
    if(m_window->state)
      m_window->state->renderer = {};
    else
      qDebug() << "?? ";
    ////window->state->hasSwapChain = false;
  }
}

void ScreenNode::setRenderer(std::shared_ptr<RenderList> r)
{
  m_window->state->renderer = r;
}

RenderList* ScreenNode::renderer() const
{
  if(m_window && m_window->state)
    return m_window->state->renderer.lock().get();
  else
    return nullptr;
}

void ScreenNode::setScreen(QScreen* scr)
{
  m_screen = scr;
  if(m_window)
  {
    m_window->setScreen(scr);
  }
}

void ScreenNode::setPosition(QPoint pos)
{
  m_pos = pos;
  if(m_window)
  {
    m_window->setPosition(pos);
  }
}

void ScreenNode::setSize(QSize sz)
{
  m_sz = sz;
  if(m_window)
  {
    m_window->setGeometry({m_window->position(), sz});
  }
}

void ScreenNode::setRenderSize(QSize sz)
{
  if(sz.width() >= 1 && sz.height() >= 1)
    m_renderSz = sz;
  else
    m_renderSz = std::nullopt;

  if(m_window && m_window->onResize)
  {
    m_window->onResize();
  }
}

void ScreenNode::setFullScreen(bool b)
{
  m_fullScreen = b;
  if(m_window)
  {
    if(b)
    {
      m_window->showFullScreen();
    }
    else
    {
      m_window->showNormal();
    }
  }
}

void ScreenNode::createOutput(
    GraphicsApi graphicsApi, std::function<void()> onReady,
    std::function<void()> onUpdate, std::function<void()> onResize)
{
  if(m_ownsWindow)
  {
    m_window = std::make_shared<Window>(graphicsApi);
    if(m_embedded)
      m_window->unsetCursor();
  }

  QObject::connect(m_window.get(), &Window::mouseMove, [this](QPointF s, QPointF w) {
    if(onMouseMove)
      onMouseMove(s, w);
  });
  QObject::connect(m_window.get(), &Window::tabletMove, [this](QTabletEvent* e) {
    if(onTabletMove)
      onTabletMove(e);
  });
  QObject::connect(m_window.get(), &Window::key, [this](int k, const QString& t) {
    if(onKey)
      onKey(k, t);
  });
  m_window->onUpdate = std::move(onUpdate);
  m_window->onWindowReady = [this, graphicsApi, onReady = std::move(onReady)] {
    m_window->state = createRenderState(*m_window, graphicsApi);
    m_window->state->renderSize = QSize(1280, 720);
    if(m_window->state->rhi)
    {
      // TODO depth stencil, render buffer, etc ?
      m_swapChain = m_window->state->rhi->newSwapChain();
      m_swapChain->setName("ScreenNode::m_swapChain");
      m_window->m_swapChain = m_swapChain;
      m_depthStencil = m_window->state->rhi->newRenderBuffer(
          QRhiRenderBuffer::DepthStencil,
          QSize(), // no need to set the size here, due to UsedWithSwapChainOnly
          m_window->state->samples, QRhiRenderBuffer::UsedWithSwapChainOnly);
      m_depthStencil->setName("ScreenNode::m_depthStencil");
      m_swapChain->setWindow(m_window.get());
      m_swapChain->setDepthStencil(m_depthStencil);
      m_swapChain->setSampleCount(m_window->state->samples);

      QRhiSwapChain::Flags flags = QRhiSwapChain::MinimalBufferCount;
      if(!score::AppContext().settings<Gfx::Settings::Model>().getVSync())
        flags |= QRhiSwapChain::NoVSync;
      m_swapChain->setFlags(flags);

      m_window->state->renderPassDescriptor
          = m_swapChain->newCompatibleRenderPassDescriptor();
      m_swapChain->setRenderPassDescriptor(m_window->state->renderPassDescriptor);

      onReady();
    }
  };
  m_window->onResize = [this, onResize = std::move(onResize)] {
    if(m_window && m_window->state)
    {
      auto& st = *m_window->state;
      if(!this->m_renderSz)
      {
        st.renderSize = st.outputSize;
      }
      else
      {
        st.renderSize = *this->m_renderSz;
      }
    }

    if(onResize)
    {
      onResize();
    }
  };

  if(!m_embedded)
  {
    /*
    if(window->isExposed())
    {
      window->exposeEvent(nullptr);
    }
    */

    if(m_screen)
    {
      m_window->setScreen(m_screen);
    }

    if(m_fullScreen)
    {
      m_window->showFullScreen();
    }
    else
    {
      if(m_pos)
      {
        m_window->setPosition(*m_pos);
      }

      if(!m_sz)
      {
        m_window->resize(1280, 720);
      }
      else
      {
        m_window->setGeometry({m_window->position(), *m_sz});
      }
      m_window->show();
    }
  }
}

void ScreenNode::destroyOutput()
{
  if(!m_window)
    return;

  delete m_depthStencil;
  m_depthStencil = nullptr;

  if(auto s = m_window->state)
  {
    delete s->renderPassDescriptor;
    s->renderPassDescriptor = nullptr;
  }

  //delete s.renderBuffer;
  //s.renderBuffer = nullptr;

  delete m_swapChain;
  m_swapChain = nullptr;
  m_window->m_swapChain = nullptr;

  if(auto s = m_window->state)
  {
    s->destroy();
  }

  if(m_ownsWindow)
  {
    m_window.reset();
  }
}

void ScreenNode::updateGraphicsAPI(GraphicsApi api)
{
  if(!m_window)
    return;

  if(m_window->api() != api)
  {
    destroyOutput();
  }
  else if(this->m_window)
  {
    if(this->m_window->state)
    {
      const int samples
          = score::AppContext().settings<Gfx::Settings::Model>().getSamples();

      if(this->m_window->state->samples != samples)
      {
        destroyOutput();
      }
    }
  }
}

std::shared_ptr<score::gfx::RenderState> ScreenNode::renderState() const
{
  if(m_window && m_window->m_swapChain)
    return m_window->state;
  return nullptr;
}

class ScreenNode::BasicRenderer : public score::gfx::OutputNodeRenderer
{
public:
  TextureRenderTarget m_rt;

  TextureRenderTarget renderTargetForInput(const Port& p) override { return m_rt; }
  BasicRenderer(const RenderState& state, const ScreenNode& parent)
      : score::gfx::OutputNodeRenderer{parent}
  {
    if(parent.m_swapChain)
    {
      m_rt.renderTarget = parent.m_swapChain->currentFrameRenderTarget();
      m_rt.renderPass = state.renderPassDescriptor;
    }
    else
    {
      m_rt.renderTarget = nullptr;
      m_rt.renderPass = nullptr;
      qDebug() << "Warning: swapchain not found in screenRenderTarget";
    }
  }

  ~BasicRenderer() { }
  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override { }
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override {
  }
  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& e) override { }
  void release(RenderList&) override { }
};

class ScreenNode::ScaledRenderer : public score::gfx::OutputNodeRenderer
{
public:
  score::gfx::TextureRenderTarget m_inputTarget;
  score::gfx::TextureRenderTarget m_renderTarget;

  QShader m_vertexS, m_fragmentS;

  std::vector<score::gfx::Sampler> m_samplers;

  score::gfx::Pipeline m_p;

  score::gfx::MeshBuffers m_mesh{};

  const ScreenNode& node() const noexcept
  {
    return static_cast<const ScreenNode&>(NodeRenderer::node);
  }
  TextureRenderTarget renderTargetForInput(const Port& p) override
  {
    return m_inputTarget;
  }
  ScaledRenderer(const RenderState& state, const ScreenNode& parent)
      : score::gfx::OutputNodeRenderer{parent}
  {
  }

  ~ScaledRenderer() { }

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    // FIXME RGBA32F for hdr ?
    m_inputTarget = score::gfx::createRenderTarget(
        renderer.state, QRhiTexture::Format::RGBA8, renderer.state.renderSize,
        renderer.samples());

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

    // Put the input texture, where all the input nodes are rendering, in a sampler.
    {
      auto sampler = renderer.state.rhi->newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

      sampler->setName("FullScreenImageNode::sampler");
      sampler->create();

      m_samplers.push_back({sampler, this->m_inputTarget.texture});
    }

    m_renderTarget.renderTarget = node().m_swapChain->currentFrameRenderTarget();
    m_renderTarget.renderPass = renderer.state.renderPassDescriptor;

    m_p = score::gfx::buildPipeline(
        renderer, mesh, m_vertexS, m_fragmentS, m_renderTarget, nullptr, nullptr,
        m_samplers);
  }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override {
  }

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& e) override
  {
    // m_rt.renderTarget = parent.m_swapChain->currentFrameRenderTarget();
    // m_rt.renderPass = state->renderPassDescriptor;
  }

  void finishFrame(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      QRhiResourceUpdateBatch*& res) override
  {
    cb.beginPass(m_renderTarget.renderTarget, Qt::black, {1.0f, 0}, res);
    res = nullptr;
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

  void release(RenderList&) override
  {
    m_p.release();
    m_inputTarget.release();
    for(auto& s : m_samplers)
    {
      delete s.sampler;
    }
    m_samplers.clear();
    m_renderTarget.release();
  }
};

score::gfx::OutputNodeRenderer* ScreenNode::createRenderer(RenderList& r) const noexcept
{
  return new ScaledRenderer{r.state, *this};
}

OutputNode::Configuration ScreenNode::configuration() const noexcept
{
  return {};
}

}
