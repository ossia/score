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
  m_renderState = score::gfx::createRenderState(
      score::gfx::GraphicsApi::OpenGL, QSize(200, 200), nullptr);
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
  // That cannot fire today, because this node hardcodes GraphicsApi::OpenGL
  // above and OpenGL has no VMA. It becomes reachable the moment this follows
  // the graph's backend the way the other output nodes do -- which is exactly
  // how it surfaced in score-addon-ndi. Calling it now costs nothing and
  // removes the trap. Idempotent.
  releaseRegistry();

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
