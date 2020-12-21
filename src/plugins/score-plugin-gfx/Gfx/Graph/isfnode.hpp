#pragma once
#include "mesh.hpp"
#include "node.hpp"
#include "renderer.hpp"

#include <isf.hpp>

#include <list>

struct RenderedISFNode;
struct ISFNode : score::gfx::ProcessNode
{
  ISFNode(const isf::descriptor& desc, const QShader& vert, const QShader& frag);
  ISFNode(const isf::descriptor& desc, const QShader& vert, const QShader& frag, const Mesh* mesh);

  virtual ~ISFNode();
  const Mesh& mesh() const noexcept;

  score::gfx::NodeRenderer* createRenderer() const noexcept;

  // Texture format: 1 row = 1 channel of N samples
  std::list<AudioTexture> audio_textures;
  std::unique_ptr<char[]> m_materialData;

private:
  friend struct RenderedISFNode;
  const Mesh* m_mesh{};

  QShader m_vertexS;
  QShader m_fragmentS;
  isf::descriptor m_descriptor;
  int m_materialSize{};
};
