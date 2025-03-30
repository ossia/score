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
 *
 * See https://isf.video
 */
class SCORE_PLUGIN_GFX_EXPORT ISFNode : public score::gfx::ProcessNode
{
public:
  ISFNode(const isf::descriptor& desc, const QString& vert, const QString& frag);

  virtual ~ISFNode();
  QSize computeTextureSize(const isf::pass& pass, QSize origSize);

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  const isf::descriptor& descriptor() const noexcept { return m_descriptor; }

  friend SinglePassISFNode;
  friend RenderedISFNode;
  friend isf_input_port_vis;

  isf::descriptor m_descriptor;

  // Texture format: 1 row = 1 channel of N samples
  std::list<AudioTexture> m_audio_textures;
  std::unique_ptr<char[]> m_material_data;

  QString m_vertexS;
  QString m_fragmentS;
  int m_materialSize{};
};
}
