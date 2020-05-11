#include "filternode.hpp"
#include "isfnode.hpp"
#include "window.hpp"

NodeModel::~NodeModel() {}

RenderedNode* NodeModel::createRenderer() const noexcept
{
  return new RenderedNode{*this};
}

ColorNode::~ColorNode() {}

ScreenNode::~ScreenNode() {}

ProductNode::~ProductNode() {}

NoiseNode::~NoiseNode() {}

FilterNode::~FilterNode() {}

ISFNode::~ISFNode() {}


// extern "C" float __log_finite(float f ) { return std::log(f); }
