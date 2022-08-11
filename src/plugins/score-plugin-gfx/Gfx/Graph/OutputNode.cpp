#include <Gfx/Graph/OutputNode.hpp>
namespace score::gfx
{

OutputNode::OutputNode() { }

OutputNode::~OutputNode() { }

void OutputNode::updateGraphicsAPI(GraphicsApi) { }

OutputNodeRenderer::~OutputNodeRenderer() { }

void OutputNodeRenderer::finishFrame(RenderList&, QRhiCommandBuffer& commands) { }

}
