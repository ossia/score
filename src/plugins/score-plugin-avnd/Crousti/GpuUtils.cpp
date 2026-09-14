#if SCORE_PLUGIN_GFX

#include "GpuUtils.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <score/gfx/OpenGL.hpp>
#include <score/gfx/Vulkan.hpp>

#include <QOffscreenSurface>

#include <private/qrhigles2_p.h>

namespace oscr
{
static void
customMessageProcess(const score::gfx::Message& msg, score::gfx::Message& last_message)
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

CustomGfxNodeBase::~CustomGfxNodeBase() = default;
CustomGfxOutputNodeBase::~CustomGfxOutputNodeBase() = default;

CustomGpuOutputNodeBase::CustomGpuOutputNodeBase(
    std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls&& ctls,
    const score::DocumentContext& ctx)
    : GpuControlOuts{std::move(q), std::move(ctls)}
    , m_ctx{ctx}
{
}

CustomGpuOutputNodeBase::~CustomGpuOutputNodeBase()
{
  // The GpuResourceRegistry owns QRhiBuffer wrappers, and RenderList::init
  // acquires one for every OutputNode whether the node asks or not. Their
  // destructors have to run while the QRhi is still alive, which is what
  // score::gfx::OutputNode::releaseRegistry() is for -- "concrete subclasses
  // MUST call this from destroyOutput() BEFORE the QRhi is torn down".
  //
  // Without it the allocator still holds live blocks at teardown:
  //
  //   ASSERT "Some allocations were not freed before destruction of this
  //           memory block!"  (vk_mem_alloc.h)
  //
  // This follows the graph's backend, so the QRhi here can be a Vulkan one --
  // which is how it surfaced in score-addon-ndi. Idempotent.
  releaseRegistry();

  if(m_renderState)
    m_renderState->destroy();
}

void CustomGpuOutputNodeBase::process(score::gfx::Message&& msg)
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

void CustomGpuOutputNodeBase::startRendering() { }

void CustomGpuOutputNodeBase::render()
{
  auto renderer = m_renderer.lock();
  if(renderer && m_renderState)
  {
    auto rhi = m_renderState->rhi;
    QRhiCommandBuffer* cb{};
    if(rhi->beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      return;

    renderer->render(*cb, true);
    rhi->endOffscreenFrame();
  }
}

void CustomGpuOutputNodeBase::stopRendering() { }

bool CustomGpuOutputNodeBase::canRender() const
{
  return true;
}

void CustomGpuOutputNodeBase::onRendererChange() { }

void CustomGpuOutputNodeBase::createOutput(score::gfx::OutputConfiguration conf)
{
  // Nothing here fixes a resolution or a format the way a window or an encoder
  // does: this node is a sink that reads back what reaches it. So the graph is
  // rendered at whatever its first texture input asks for, and the backend is
  // the one the settings picked -- building an OpenGL device of its own would
  // leave the rest of the score on Vulkan or Metal.
  QSize size{defaultRenderSize};
  auto format = QRhiTexture::RGBA8;
  if(auto spec = firstInputRenderTargetSpecs())
  {
    if(!spec->size.isEmpty())
      size = spec->size;
    format = spec->format;
  }

  m_renderState = score::gfx::createRenderState(conf.graphicsApi, size, nullptr);
  if(!m_renderState || !m_renderState->rhi)
  {
    qWarning() << "CustomGpuOutputNode: could not create a render state";
    m_renderState.reset();
    return;
  }
  m_renderState->outputSize = m_renderState->renderSize;
  m_renderState->renderFormat = format;

  if(conf.onReady)
    conf.onReady();
}

void CustomGpuOutputNodeBase::destroyOutput() { }

std::shared_ptr<score::gfx::RenderState> CustomGpuOutputNodeBase::renderState() const
{
  return m_renderState;
}

score::gfx::OutputNode::Configuration
CustomGpuOutputNodeBase::configuration() const noexcept
{
  return {.manualRenderingRate = 1000. / 60., .outputNeedsRenderPass = true};
}

void CustomGfxNodeBase::process(score::gfx::Message&& msg)
{
  customMessageProcess(msg, last_message);
}

void CustomGfxOutputNodeBase::process(score::gfx::Message&& msg)
{
  customMessageProcess(msg, last_message);
}

void CustomGpuNodeBase::process(score::gfx::Message&& msg)
{
  customMessageProcess(msg, last_message);
}

}
#endif
