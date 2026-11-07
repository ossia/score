#pragma once

#include <Gfx/GfxParameter.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/InvertYRenderer.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Window/WindowSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/Debug.hpp>

namespace score::gfx
{
struct BackgroundNode : OutputNode
{
  explicit BackgroundNode(SharedDeviceMode deviceMode = SharedDeviceMode::Owned)
      : m_deviceMode{deviceMode}
  {
    input.push_back(new Port{this, {}, Types::Image, {}});
    m_conf = {
        .manualRenderingRate = 1000. / Gfx::Settings::renderRateForCurrentApplication()};
  }

  virtual ~BackgroundNode() { destroyOutput(); }

  /**
   * @brief Render at one sample whatever the anti-aliasing setting.
   *
   * For previews: small pictures in the UI, where a multisampled colour and
   * depth pair costs tens of megabytes per preview and shows nothing the
   * picture's size can carry. The preview hub renders at one sample too, so
   * a preview looks the same whichever of the two serves it. Takes effect at
   * the next createOutput.
   */
  void setSingleSample(bool b) noexcept { m_singleSample = b; }

  void startRendering() override { }
  void render() override
  {
    if(m_deviceLost)
      return;
    auto renderer = m_renderer.lock();
    if(renderer && m_renderState)
    {
      if(renderer->renderers.size() > 1)
      {
        auto& rhi = *m_renderState->rhi;
        score::gfx::OffscreenFrame frame{rhi};
        if(!frame)
        {
          checkDeviceLost(frame, rhi, "BackgroundNode");
          return;
        }

        renderer->render(frame.commands());
        frame.end();
        checkDeviceLost(frame, rhi, "BackgroundNode");
      }
      else
      {
        // Nothing upstream: the render list holds only this output. Clearing
        // leaves pixelSize at QSize(-1,-1), which WindowDevice::grabTo reports
        // as "nothing rendered ... no process is connected to this device's
        // input" -- a false blank when the graph is in fact connected, hence
        // the trace.
        static const bool trace = qEnvironmentVariableIsSet("SCORE_GFX_TRACE");
        if(trace)
          fprintf(
              stderr, "GFX-BACKGROUND readback cleared: renderers=%zu\n",
              renderer->renderers.size());
        shared_readback->data.clear();
        shared_readback->pixelSize = {};
      }
    }
  }
  void onRendererChange() override { }
  bool canRender() const override { return m_renderState && m_renderTarget; }
  void stopRendering() override { }

  void setRenderer(std::shared_ptr<RenderList> r) override { m_renderer = r; }
  RenderList* renderer() const override { return m_renderer.lock().get(); }

  void createOutput(score::gfx::OutputConfiguration conf) override
  {
    m_onResize = conf.onResize;
    m_onReleaseRenderList = conf.onReleaseRenderList;
    // Cache the requested graphics API so setSwapchainFormat can rebuild
    // through createOutput when the format changes (live HDR↔SDR toggle):
    // setting m_swapchainFormat alone leaves the QRhiTexture in its original
    // format.
    m_lastGraphicsApi = conf.graphicsApi;
    resetDeviceLost();

    QSize newSz = m_renderSize;
    if(newSz.width() <= 0 || newSz.height() <= 0)
      newSz = m_size;
    if(newSz.width() <= 0 || newSz.height() <= 0)
      newSz = QSize{1024, 1024};

    m_renderState = score::gfx::createRenderState(
        conf.graphicsApi, newSz, nullptr, m_deviceMode);
    if(!m_renderState || !m_renderState->rhi)
    {
      qWarning() << "BackgroundNode: failed to create QRhi";
      m_renderState.reset();
      return;
    }
    m_renderState->outputSize = m_renderState->renderSize;
    if(m_singleSample)
      m_renderState->samples = 1;
    m_renderState->renderFormat
        = (m_swapchainFormat != Gfx::SwapchainFormat::SDR)
              ? QRhiTexture::RGBA32F
              : QRhiTexture::RGBA8;

    auto rhi = m_renderState->rhi;
    m_texture = rhi->newTexture(
        m_renderState->renderFormat, m_renderState->renderSize, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    m_texture->create();

    // Reverse-Z project rule: the depth attachment is D32F (float). Fixed-point
    // D24 combined with reverse-Z gives strictly worse precision than
    // standard-Z.
    m_renderTarget
        = createOutputRenderTarget(*m_renderState, *m_texture, OutputDepth::Texture);

    conf.onReady();
  }

  void destroyOutput() override
  {
    if(m_renderState)
    {
      // Drain the GPU before tearing resources down. Same rationale as
      // ScreenNode::destroyOutput: when setSwapchainFormat invokes
      // destroyOutput synchronously, an unfinished cbWrapper from a prior
      // offscreen frame can still be referenced by ScenePreprocessor's
      // per-frame copyBuffer. Recording into that CB after we've
      // freed the rhi triggers VUID-vkCmdCopyBuffer-commandBuffer-
      // recording and a device loss.
      if(m_renderState->rhi)
      {
        SCORE_ASSERT(!m_renderState->rhi->isRecordingFrame());
        m_renderState->rhi->finish();
      }

      // The registry survives render list teardown, so its QRhi resources
      // must be released here, before RenderState::destroy() tears down the
      // QRhi: destroyOwned() `delete`s the wrappers while the device is alive.
      releaseRegistry();

      releaseOutputRenderTarget(*m_renderState, m_renderTarget);

      delete m_texture;
      m_texture = nullptr;

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

    // Live format change while rendering: m_texture was allocated by
    // createOutput with the prior format, and setFormat alone does not
    // re-allocate its GPU memory backing. Route through destroyOutput +
    // createOutput so the render target, RPD, depth and colour textures all
    // come back in the matching format. With no output yet (m_renderState
    // null) createOutput picks the format up from m_swapchainFormat.
    if(m_renderState)
    {
      score::gfx::OutputConfiguration conf;
      conf.graphicsApi = m_lastGraphicsApi;
      conf.onResize = m_onResize;
      conf.onReleaseRenderList = m_onReleaseRenderList;
      // The Graph-owned RenderList holds QRhiResources belonging to the QRhi
      // destroyOutput() is about to `delete`; release it first.
      releaseOwnedRenderList();
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

      // Drain the GPU before destroying the render target and m_texture, as
      // destroyOutput does: the current frame's offscreen CB, or a queued one,
      // may still reference these resources, and the setPixelSize + create
      // below does not drain internally. Without this, validation fires on
      // the next vkCmd* recording (-recording / -commandBuffer-recording /
      // -in-use) and can lose the device.
      rhi->finish();

      if(m_renderTarget.renderTarget)
        m_renderTarget.renderTarget->destroy();
      releaseOutputRenderTarget(*m_renderState, m_renderTarget, true);
      m_texture->destroy();
      m_texture->setPixelSize(newSz);
      m_texture->create();

      m_renderTarget
          = createOutputRenderTarget(*m_renderState, *m_texture, OutputDepth::Texture);
    }

    if(m_onResize)
      m_onResize();
  }

  std::shared_ptr<RenderState> renderState() const override { return m_renderState; }

  score::gfx::TextureRenderTarget currentRenderTarget() const noexcept override
  {
    if(!m_renderState)
      return {};
    return m_renderTarget;
  }

  score::gfx::OutputNodeRenderer* createRenderer(RenderList& r) const noexcept override
  {
    return new Gfx::InvertYRenderer{
        *this, currentRenderTarget(),
        const_cast<QRhiReadbackResult&>(*shared_readback)};
  }

  OutputNode::Configuration configuration() const noexcept override { return m_conf; }

  std::shared_ptr<QRhiReadbackResult> shared_readback;

private:
  Configuration m_conf;
  SharedDeviceMode m_deviceMode{SharedDeviceMode::Owned};
  bool m_singleSample{};

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  QRhiTexture* m_texture{};
  score::gfx::TextureRenderTarget m_renderTarget;
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
