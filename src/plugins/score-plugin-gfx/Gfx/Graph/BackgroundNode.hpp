#pragma once

#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>

namespace score::gfx
{
struct BackgroundNode2 : OutputNode
{
  explicit BackgroundNode2()
  {
    input.push_back(new Port{this, {}, Types::Image, {}});
    auto& ctx = score::GUIAppContext();
    auto& settings = ctx.settings<Gfx::Settings::Model>();
    double settings_rate = settings.getRate();
    if(settings_rate <= 0.)
      settings_rate = 60.;
    m_conf = {.manualRenderingRate = 1000. / settings_rate};
  }

  virtual ~BackgroundNode2() { }

  void startRendering() override { qDebug(Q_FUNC_INFO); }
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
  void onRendererChange() override { qDebug(Q_FUNC_INFO); }
  bool canRender() const override { return true; }
  void stopRendering() override { }

  void setRenderer(std::shared_ptr<RenderList> r) override { m_renderer = r; }
  RenderList* renderer() const override { return m_renderer.lock().get(); }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_renderState
        = score::gfx::createRenderState(conf.graphicsApi, QSize{1024, 1024}, nullptr);

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
};
}
