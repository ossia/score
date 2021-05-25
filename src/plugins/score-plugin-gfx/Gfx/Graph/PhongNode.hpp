#pragma once
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>

namespace score::gfx
{

struct PhongNode : NodeModel
{
  PhongNode(const Mesh* mesh);

  virtual ~PhongNode();
  const Mesh& mesh() const noexcept;

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept;

private:
  const Mesh* m_mesh{};
};

}
