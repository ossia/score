#pragma once
#include "mesh.hpp"
#include "node.hpp"
#include "renderer.hpp"

#include <isf.hpp>

#include <list>

struct RenderedISFNode;
struct ISFNode : score::gfx::ProcessNode
{
  ISFNode(
      const isf::descriptor& desc,
      const QShader& vert,
      const QShader& frag);
  ISFNode(
      const isf::descriptor& desc,
      const QShader& vert,
      const QShader& frag,
      const Mesh* mesh);

  virtual ~ISFNode();
  const Mesh& mesh() const noexcept;
  QSize computeTextureSize(const isf::pass& pass, QSize origSize);

  score::gfx::NodeRenderer* createRenderer(Renderer& r) const noexcept;

  isf::descriptor descriptor;

  // Texture format: 1 row = 1 channel of N samples
  std::list<AudioTexture> audio_textures;
  std::unique_ptr<char[]> m_materialData;

private:
  friend struct SinglePassISFNode;
  friend struct RenderedISFNode;
  const Mesh* m_mesh{};

  QShader m_vertexS;
  QShader m_fragmentS;
  int m_materialSize{};
};
