#pragma once

#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

namespace score::gfx
{
struct BackgroundNode : OutputNode
{
  explicit BackgroundNode()
  {
    input.push_back(new Port{this, {}, Types::Image, {}});
    auto& ctx = score::GUIAppContext();
    auto& settings = ctx.settings<Gfx::Settings::Model>();
    double settings_rate = settings.getRate();
    if(settings_rate <= 0.)
      settings_rate = 60.;
    m_conf = {.manualRenderingRate = 1000. / settings_rate};
  }

  virtual ~BackgroundNode() { }

  void startRendering() override { }
  void render() override
  {
    auto renderer = m_renderer.lock();
    if(renderer && m_renderState)
    {
      if(renderer->renderers.size() > 1)
      {
        auto rhi = m_renderState->rhi;
        QRhiCommandBuffer* cb{};
        if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
          return;

        renderer->render(*cb);
        rhi->endOffscreenFrame();
      }
      else
      {
        shared_readback->data.clear();
        shared_readback->pixelSize = {};
      }
    }
  }
  void onRendererChange() override { }
  bool canRender() const override { return true; }
  void stopRendering() override { }

  void setRenderer(std::shared_ptr<RenderList> r) override { m_renderer = r; }
  RenderList* renderer() const override { return m_renderer.lock().get(); }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_onResize = conf.onResize;

    QSize newSz = m_renderSize;
    if(newSz.width() <= 0 || newSz.height() <= 0)
      newSz = m_size;
    if(newSz.width() <= 0 || newSz.height() <= 0)
      newSz = QSize{1024, 1024};

    m_renderState = score::gfx::createRenderState(conf.graphicsApi, newSz, nullptr);
    m_renderState->outputSize = m_renderState->renderSize;

    auto rhi = m_renderState->rhi;
    m_texture = rhi->newTexture(
        QRhiTexture::RGBA8, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();
    m_renderTarget = rhi->newTextureRenderTarget({m_texture});
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
    m_renderTarget->create();

    conf.onReady();
  }

  void destroyOutput() override { }
  void updateGraphicsAPI(GraphicsApi) override { }

  void setSize(QSize newSz)
  {
    if(m_size != newSz)
    {
      m_size = newSz;
      resize();
    }
  }
  void setRenderSize(QSize newSz)
  {
    if(m_renderSize != newSz)
    {
      m_renderSize = newSz;
      resize();
    }
  }

  void resize()
  {
    QSize newSz = m_renderSize;
    if(newSz.width() <= 0 || newSz.height() <= 0)
      newSz = m_size;
    if(newSz.width() <= 0 || newSz.height() <= 0)
      newSz = QSize{1024, 1024};

    if(m_renderState)
    {
      m_renderState->renderSize = newSz;
      m_renderState->outputSize = newSz;

      auto rhi = m_renderState->rhi;

      m_renderTarget->destroy();
      m_texture->destroy();
      m_texture->setPixelSize(newSz);
      m_texture->create();
      m_renderTarget = rhi->newTextureRenderTarget({m_texture});
      m_renderState->renderPassDescriptor
          = m_renderTarget->newCompatibleRenderPassDescriptor();
      m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
      m_renderTarget->create();
    }

    if(m_onResize)
      m_onResize();
  }

  std::shared_ptr<RenderState> renderState() const override { return m_renderState; }

  score::gfx::OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override
  {
    score::gfx::TextureRenderTarget rt{
        m_texture, nullptr, nullptr, m_renderState->renderPassDescriptor,
        m_renderTarget};
    return new Gfx::InvertYRenderer{
        *this, rt, const_cast<QRhiReadbackResult&>(*shared_readback)};
  }

  OutputNode::Configuration configuration() const noexcept override { return m_conf; }

  std::shared_ptr<QRhiReadbackResult> shared_readback;

private:
  Configuration m_conf;

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};

  std::function<void()> m_onResize;
  QSize m_size{1024, 1024};
  QSize m_renderSize{};
};
}
