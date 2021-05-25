#include <Gfx/Graph/Node.hpp>

#include <Gfx/Graph/NodeRenderer.hpp>

namespace score::gfx
{
Node::Node() { }

Node::~Node() { }

NodeModel::NodeModel() { }

NodeModel::~NodeModel() { }

score::gfx::NodeRenderer* NodeModel::createRenderer(RenderList& r) const noexcept
{
  return new GenericNodeRenderer{*this};
}
}
