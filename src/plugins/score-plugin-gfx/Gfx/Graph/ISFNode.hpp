#pragma once
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <isf.hpp>

#include <list>
namespace score::gfx
{
struct SinglePassISFNode;
struct RenderedISFNode;
struct isf_input_port_vis;
/**
 * @brief Data model for Interactive Shader Format filters.
 */
class ISFNode : public score::gfx::ProcessNode
{
public:
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

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept;

  const isf::descriptor& descriptor() const noexcept { return m_descriptor; }

private:
  friend SinglePassISFNode;
  friend RenderedISFNode;
  friend isf_input_port_vis;

  isf::descriptor m_descriptor;

  // Texture format: 1 row = 1 channel of N samples
  std::list<AudioTexture> m_audio_textures;
  std::unique_ptr<char[]> m_material_data;

  const Mesh* m_mesh{};
  QShader m_vertexS;
  QShader m_fragmentS;
  int m_materialSize{};
};
}
