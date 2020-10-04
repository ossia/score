#pragma once
#include "mesh.hpp"
#include "node.hpp"
#include "renderer.hpp"

struct PhongNode : NodeModel
{
  PhongNode(const Mesh* mesh);

  virtual ~PhongNode();
  const Mesh& mesh() const noexcept;

  RenderedNode* createRenderer() const noexcept;

private:
  const Mesh* m_mesh{};
};
