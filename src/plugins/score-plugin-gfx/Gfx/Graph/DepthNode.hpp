#pragma once
#include "mesh.hpp"
#include "node.hpp"
#include "renderer.hpp"

namespace score::gfx
{
struct RenderedDepthNode;
struct DepthNode : score::gfx::ProcessNode
{
  DepthNode(const QShader& compute);

  virtual ~DepthNode();

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept;

private:
  friend struct RenderedISFNode;
  QShader m_computeS;
};
}
