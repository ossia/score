#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/OutputNode.hpp>
namespace score::gfx
{

OutputNode::OutputNode() { }

OutputNode::~OutputNode() { }

void OutputNode::updateGraphicsAPI(GraphicsApi) { }

void OutputNode::releaseOwnedRenderList()
{
  // Copy before invoking: the Graph is free to rebuild this output from
  // inside the callback, which reassigns m_onReleaseRenderList — destroying
  // the std::function whose operator() is on the stack.
  if(auto cb = m_onReleaseRenderList)
    cb();
}

void OutputNode::setVSyncCallback(std::function<void()>) { }
OutputNodeRenderer::~OutputNodeRenderer() { }

void OutputNodeRenderer::finishFrame(
    RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res)
{
}

GpuResourceRegistry& OutputNode::acquireRegistry()
{
  // Persist-across-rebuild contract: lazy-allocated once per OutputNode.
  // RenderList::init then either calls GpuResourceRegistry::init() (first
  // RL on this OutputNode / first RL after a releaseRegistry()) or reuses
  // the populated state as-is.
  if(!m_registry)
    m_registry = std::make_unique<GpuResourceRegistry>();
  return *m_registry;
}

void OutputNode::releaseRegistry()
{
  // Concrete subclasses MUST call this from destroyOutput() BEFORE the
  // QRhi is torn down. destroyOwned() `delete`s the QRhiBuffer /
  // QRhiTexture / QRhiSampler wrappers directly, so the QRhi must still
  // be alive to honour the QRhiResource destructors.
  if(m_registry)
  {
    m_registry->destroyOwned();
    m_registry.reset();
  }
}

}
