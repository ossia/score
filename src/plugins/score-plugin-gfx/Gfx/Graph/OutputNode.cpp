#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/OutputNode.hpp>
namespace score::gfx
{

OutputNode::OutputNode() { }

OutputNode::~OutputNode() { }

void OutputNode::updateGraphicsAPI(GraphicsApi) { }
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
  // the populated state as-is (every subsequent rebuild — what we want
  // for the resize fast path).
  if(!m_registry)
    m_registry = std::make_unique<GpuResourceRegistry>();
  return *m_registry;
}

void OutputNode::releaseRegistry()
{
  // Concrete subclasses MUST call this from destroyOutput() BEFORE the
  // QRhi is torn down. destroyOwned() `delete`s the QRhiBuffer /
  // QRhiTexture / QRhiSampler wrappers directly (no deleteLater path —
  // the registry has outlived the RenderList that used to plumb
  // releaseBuffer for it), so the QRhi must still be alive to honour the
  // QRhiResource destructors.
  if(m_registry)
  {
    m_registry->destroyOwned();
    m_registry.reset();
  }
}

}
