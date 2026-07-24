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
class ISFNode : public score::gfx::ProcessNode
{
public:
  ISFNode(const isf::descriptor& desc, const QString& vert, const QString& frag);
  ISFNode(const isf::descriptor& desc, const QString& comp);

  virtual ~ISFNode();
  QSize computeTextureSize(const isf::pass& pass, QSize origSize);

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override;

  const isf::descriptor& descriptor() const noexcept { return m_descriptor; }
  void process(Message&& msg) override;
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
  std::vector<int*> m_event_ports;

  int m_materialSize{};

  // Reset all `event` input ports to 0 so they pulse true for exactly one
  // frame after the upstream producer writes 1. Called at the end of each
  // frame's update() — AFTER the material UBO has been staged via
  // updateDynamicBuffer (which captures the value at call time), so
  // resetting the CPU memory here doesn't affect what the shader reads
  // this frame, only what would leak into the next frame if we didn't
  // reset.
  //
  // Returns true if any port was actually firing. Callers should then set
  // their NodeRenderer::materialChanged flag so the next frame re-uploads
  // the now-zero event value — otherwise the gate-on-materialChanged
  // upload path would skip the re-upload and leave the stale 1 in the GPU
  // UBO indefinitely.
  [[nodiscard]] bool resetEventPortsAfterFrame() noexcept
  {
    bool any_fired = false;
    for(int* p : m_event_ports)
    {
      if(p && *p != 0)
      {
        *p = 0;
        any_fired = true;
      }
    }
    return any_fired;
  }
};
}
