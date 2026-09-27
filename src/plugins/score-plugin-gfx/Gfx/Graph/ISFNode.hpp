#pragma once
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <isf.hpp>

#include <score_plugin_gfx_export.h>

#include <list>
#include <span>
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
  ISFNode(const isf::descriptor& desc, const QString& comp);

  virtual ~ISFNode();
  QSize computeTextureSize(
      const isf::pass& pass, QSize origSize,
      std::span<const Sampler> inputSamplers = {});

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  const isf::descriptor& descriptor() const noexcept { return m_descriptor; }

  // A raw raster whose shader reads the `camera` block gets one extra Scene
  // input, after every INPUTS port: a Camera wired there fills that block.
  static bool hasCameraInput(const isf::descriptor& desc) noexcept;
  int cameraInput() const noexcept { return m_cameraInput; }
  void process(Message&& msg) override;
  //! An event input fires for one frame on true -- and on an impulse, which
  //! the generic port writer ignores: a cable or a message could not fire it.
  void process(int32_t port, const ossia::value& v) override;
  using ProcessNode::process;
  friend SinglePassISFNode;
  friend RenderedISFNode;
  friend isf_input_port_vis;

  isf::descriptor m_descriptor;

  // Texture format: 1 row = 1 channel of N samples
  std::list<AudioTexture> m_audio_textures;
  std::unique_ptr<char[]> m_material_data;

  QString m_vertexS;
  QString m_fragmentS;
  QString m_computeS;

  int m_cameraInput{-1};
  int m_materialSize{};
  // Bytes of m_material_data the shader's material UBO actually covers. USER
  // dispatch ports are appended past this: their values are read on the CPU to
  // size the dispatch, and no generated shader declares them.
  int m_materialUBOSize{};
};
}
