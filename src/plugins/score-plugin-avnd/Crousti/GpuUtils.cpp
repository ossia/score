#if SCORE_PLUGIN_GFX

#include "GpuUtils.hpp"
#include <score/gfx/Vulkan.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <private/qrhigles2_p.h>
namespace oscr
{
static void customMessageProcess(const score::gfx::Message& msg, score::gfx::Message& last_message)
{
  //ProcessNode::process(msg.token);
  last_message.token = msg.token;
  if(last_message.input.empty())
  {
    last_message = msg;
  }
  else
  {
    for(std::size_t i = 0; i < msg.input.size(); i++)
    {
      // If there's some data, overwrite it
      if(msg.input[i].index() != 0)
        last_message.input[i] = msg.input[i];
    }
  }
}

CustomGpuOutputNodeBase::CustomGpuOutputNodeBase()
{
  m_renderState = std::make_shared<score::gfx::RenderState>();

  m_renderState->surface = QRhiGles2InitParams::newFallbackSurface();
  QRhiGles2InitParams params;
  params.fallbackSurface = m_renderState->surface;

#include <Gfx/Qt5CompatPop> // clang-format: keep
  m_renderState->rhi = QRhi::create(QRhi::Vulkan, &params, QRhi::EnableDebugMarkers);
#include <Gfx/Qt5CompatPush> // clang-format: keep

  m_renderState->size = QSize(1000, 1000);
  m_renderState->api = score::gfx::GraphicsApi::Vulkan;
}

void CustomGpuOutputNodeBase::process(const score::gfx::Message& msg)
{
  customMessageProcess(msg, last_message);
}

void CustomGpuOutputNodeBase::setRenderer(std::shared_ptr<score::gfx::RenderList> r)
{

  m_renderer = r;
}

score::gfx::RenderList* CustomGpuOutputNodeBase::renderer() const
{
  return m_renderer.lock().get();
}

void CustomGpuOutputNodeBase::startRendering()
{

}

void CustomGpuOutputNodeBase::render() {

  if (m_update)
    m_update();

  auto renderer = m_renderer.lock();
  if (renderer && m_renderState)
  {
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if (rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;

    renderer->render(*cb, true);
    rhi->endOffscreenFrame();
  }
}

void CustomGpuOutputNodeBase::stopRendering() {
}

bool CustomGpuOutputNodeBase::canRender() const {
  return true;
}

void CustomGpuOutputNodeBase::onRendererChange() {
}

void CustomGpuOutputNodeBase::createOutput(
    score::gfx::GraphicsApi graphicsApi
    , std::function<void ()> onReady
    , std::function<void ()> onUpdate
    , std::function<void ()> onResize
    ) {
  m_update = onUpdate;
  onReady();
}

void CustomGpuOutputNodeBase::destroyOutput()
{
}

score::gfx::RenderState* CustomGpuOutputNodeBase::renderState() const {
  return m_renderState.get();
}

score::gfx::OutputNode::Configuration CustomGpuOutputNodeBase::configuration() const noexcept {
  return {
    .manualRenderingRate = 1000. / 60.
  , .outputNeedsRenderPass = true
  };
}

void CustomGpuNodeBase::process(const score::gfx::Message& msg)
{
  customMessageProcess(msg, last_message);
}

}
#endif
