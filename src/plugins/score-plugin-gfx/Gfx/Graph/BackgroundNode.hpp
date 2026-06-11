#pragma once

#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Window/WindowSettings.hpp>

#include <score/tools/Debug.hpp>

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

  virtual ~BackgroundNode() { destroyOutput(); }

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
    // Cache the requested graphics API so setSwapchainFormat can rebuild
    // through createOutput when the format actually changes (live HDR↔SDR
    // toggle). Without this the format setter was inert: m_swapchainFormat
    // was updated but the underlying QRhiTexture stayed in its original
    // format, silently downgrading HDR to SDR.
    m_lastGraphicsApi = conf.graphicsApi;

    QSize newSz = m_renderSize;
    if(newSz.width() <= 0 || newSz.height() <= 0)
      newSz = m_size;
    if(newSz.width() <= 0 || newSz.height() <= 0)
      newSz = QSize{1024, 1024};

    m_renderState = score::gfx::createRenderState(conf.graphicsApi, newSz, nullptr);
    if(!m_renderState || !m_renderState->rhi)
    {
      qWarning() << "BackgroundNode: failed to create QRhi";
      m_renderState.reset();
      return;
    }
    m_renderState->outputSize = m_renderState->renderSize;
    m_renderState->renderFormat
        = (m_swapchainFormat != Gfx::SwapchainFormat::SDR)
              ? QRhiTexture::RGBA32F
              : QRhiTexture::RGBA8;

    auto rhi = m_renderState->rhi;
    m_texture = rhi->newTexture(
        m_renderState->renderFormat, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();

    // Reverse-Z project rule: depth attachment is D32F (float). Fixed-point
    // D24 combined with reverse-Z gives strictly worse precision than
    // standard-Z, so we must allocate a float texture here. RenderTarget
    // flag is required for attaching as a depth target.
    m_depthTexture = rhi->newTexture(
        QRhiTexture::D32F, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget);
    m_depthTexture->setName("BackgroundNode::m_depthTexture");
    m_depthTexture->create();

    QRhiTextureRenderTargetDescription desc;
    desc.setColorAttachments({QRhiColorAttachment(m_texture)});

    desc.setDepthTexture(m_depthTexture);
    m_renderTarget = rhi->newTextureRenderTarget(desc);
    m_renderState->renderPassDescriptor
        = m_renderTarget->newCompatibleRenderPassDescriptor();
    m_renderTarget->setRenderPassDescriptor(m_renderState->renderPassDescriptor);
    m_renderTarget->create();

    conf.onReady();
  }

  void destroyOutput() override
  {
    if(m_renderState)
    {
      // Drain the GPU before tearing resources down. Same rationale as
      // ScreenNode::destroyOutput: when setSwapchainFormat invokes
      // destroyOutput synchronously (C-16 / commit e2afe7874), an
      // unfinished cbWrapper from a prior offscreen frame can still be
      // referenced by ScenePreprocessor's per-frame copyBuffer
      // (C-01 / commit fe146c8de). Recording into that CB after we've
      // freed the rhi triggers VUID-vkCmdCopyBuffer-commandBuffer-
      // recording and a device loss. Mirrors MultiWindowNode.cpp:1068.
      if(m_renderState->rhi)
      {
        // Pre-condition: destroyOutput must not be called inside a
        // frame. Mirrors ScreenNode::destroyOutput.
        SCORE_ASSERT(!m_renderState->rhi->isRecordingFrame());
        m_renderState->rhi->finish();
      }

      // Persist-across-rebuild contract: the registry survives RL
      // teardown, so we must release its QRhi resources here BEFORE
      // RenderState::destroy() tears down the QRhi. destroyOwned()
      // `delete`s the wrappers directly while the device is alive.
      releaseRegistry();

      delete m_renderTarget;
      m_renderTarget = nullptr;

      delete m_depthTexture;
      m_depthTexture = nullptr;

      delete m_texture;
      m_texture = nullptr;

      delete m_renderState->renderPassDescriptor;
      m_renderState->renderPassDescriptor = nullptr;

      m_renderState->destroy();
      m_renderState.reset();
    }
  }
  void updateGraphicsAPI(GraphicsApi api) override
  {
    if(!m_renderState)
      return;
    if(m_renderState->api != api)
      destroyOutput();
  }

  void setSwapchainFormat(Gfx::SwapchainFormat format)
  {
    if(m_swapchainFormat == format)
      return;
    m_swapchainFormat = format;

    // Live format change while rendering: the existing m_texture was
    // allocated at createOutput-time with the prior format. setFormat alone
    // wouldn't re-allocate the GPU memory backing — only setPixelSize +
    // recreate-via-resize does. Re-route through destroyOutput +
    // createOutput so the renderTarget / RPD / depth tex / colour tex all
    // come back in matching format. Skipped before any output exists
    // (m_renderState null) — createOutput will pick up the new format
    // naturally via m_swapchainFormat.
    if(m_renderState)
    {
      score::gfx::OutputConfiguration conf;
      conf.graphicsApi = m_lastGraphicsApi;
      conf.onResize = m_onResize;
      destroyOutput();
      createOutput(std::move(conf));
      if(m_onResize)
        m_onResize();
    }
  }

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

      // Drain the GPU before destroying m_renderTarget / m_texture /
      // m_depthTexture. Same anti-pattern that destroyOutput already
      // avoids via FIX-A: the current frame's offscreen CB (or a
      // queued one) may still reference these resources, and Qt's
      // setPixelSize+create dance below does not internally drain.
      // Without this, validation fires on the next vkCmd*-recording
      // (-recording / -commandBuffer-recording / -in-use) and may
      // device-loss.
      rhi->finish();

      m_renderTarget->destroy();
      m_texture->destroy();
      m_texture->setPixelSize(newSz);
      m_texture->create();

      if(m_depthTexture)
        m_depthTexture->destroy();
      else
        m_depthTexture = rhi->newTexture(
            QRhiTexture::D32F, newSz, 1, QRhiTexture::RenderTarget);
      m_depthTexture->setPixelSize(newSz);
      m_depthTexture->create();

      m_renderTarget->deleteLater();
      if(auto* rpd = m_renderState->renderPassDescriptor)
        rpd->deleteLater();
      m_renderState->renderPassDescriptor = nullptr;
      m_renderTarget = nullptr;

      QRhiTextureRenderTargetDescription desc;
      desc.setColorAttachments({QRhiColorAttachment(m_texture)});
      desc.setDepthTexture(m_depthTexture);
      m_renderTarget = rhi->newTextureRenderTarget(desc);
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
        .texture = m_texture,
        .renderPass = m_renderState->renderPassDescriptor,
        .renderTarget = m_renderTarget,
        .depthTexture = m_depthTexture};
    return new Gfx::InvertYRenderer{
        *this, rt, const_cast<QRhiReadbackResult&>(*shared_readback)};
  }

  OutputNode::Configuration configuration() const noexcept override { return m_conf; }

  std::shared_ptr<QRhiReadbackResult> shared_readback;

private:
  Configuration m_conf;

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  QRhiTexture* m_depthTexture{};
  QRhiTextureRenderTarget* m_renderTarget{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};

  std::function<void()> m_onResize;
  QSize m_size{1024, 1024};
  QSize m_renderSize{};
  Gfx::SwapchainFormat m_swapchainFormat{};
  // Cached graphics API from the last createOutput so setSwapchainFormat
  // can route a live format change through destroyOutput + createOutput
  // without having to re-derive it from the host.
  GraphicsApi m_lastGraphicsApi{};
};
}
