#include <Gfx/Graph/OutputNode.hpp>
namespace score::gfx
{

OutputNode::OutputNode()
{
  std::tie(m_vertexS, m_fragmentS)
      = score::gfx::makeShaders(m_mesh.defaultVertexShader(), filter);
}

OutputNode::~OutputNode() { }

void OutputNode::updateGraphicsAPI(GraphicsApi) { }

}
