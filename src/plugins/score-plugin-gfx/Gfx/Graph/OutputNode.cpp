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

}
