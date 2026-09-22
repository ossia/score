#pragma once

// A hand-built scene feeding a gfx graph, for the tests that drive the scene
// chain (scene -> ScenePreprocessor -> material preset) without a document.

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QBuffer>
#include <QImage>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace score::test::gfx::scene
{

inline std::shared_ptr<ossia::buffer_resource>
cpu_buffer(std::vector<float> data, ossia::buffer_data::usage usage)
{
  auto owned = std::make_shared<std::vector<float>>(std::move(data));
  auto res = std::make_shared<ossia::buffer_resource>();
  ossia::buffer_data bd;
  bd.data = std::shared_ptr<const void>(owned, owned->data());
  bd.byte_size = int64_t(owned->size() * sizeof(float));
  bd.usage_hint = usage;
  res->resource = bd;
  res->dirty_index = 1;
  return res;
}

/// A texture source carrying `img` as an embedded PNG, the way a loaded glTF
/// material carries its images.
inline std::shared_ptr<ossia::texture_source> png_source(const QImage& img)
{
  QByteArray png;
  {
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
  }
  auto src = std::make_shared<ossia::texture_source>();
  const auto* bytes = reinterpret_cast<const uint8_t*>(png.constData());
  src->embedded_data = std::make_shared<std::vector<uint8_t>>(bytes, bytes + png.size());
  src->mime_type = "image/png";
  return src;
}

inline std::string rgba_string(std::array<uint8_t, 4> c)
{
  return "(" + std::to_string(c[0]) + "," + std::to_string(c[1]) + ","
         + std::to_string(c[2]) + "," + std::to_string(c[3]) + ")";
}

/// Renderer of a scene producer: publishes m_scene to every downstream
/// consumer each frame, as the engine's scene producers do (consumers
/// short-circuit on state identity and version). Derived renderers only
/// decide what m_scene holds.
struct SourceRenderer : score::gfx::NodeRenderer
{
  using NodeRenderer::NodeRenderer;
  ossia::scene_spec m_scene;

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer&, QRhiResourceUpdateBatch*&,
      score::gfx::Edge& edge) override
  {
    if(!m_scene.state || !edge.sink || !edge.sink->node)
      return;
    auto rn = edge.sink->node->renderedNodes.find(&renderer);
    if(rn == edge.sink->node->renderedNodes.end())
      return;
    auto& in = edge.sink->node->input;
    auto it = std::find(in.begin(), in.end(), edge.sink);
    if(it != in.end())
      rn->second->process(int(it - in.begin()), m_scene, edge.source);
  }
  void
  runRenderPass(score::gfx::RenderList&, QRhiCommandBuffer&, score::gfx::Edge&) override
  {
  }
  void removeOutputPass(score::gfx::RenderList&, score::gfx::Edge&) override { }
  void release(score::gfx::RenderList&) override
  {
    m_scene = {};
    m_initialized = false;
  }
};

/// A node with one Scene outlet that publishes a fixed scene_state.
struct StaticSceneNode final : score::gfx::ProcessNode
{
  std::shared_ptr<ossia::scene_state> state;

  explicit StaticSceneNode(std::shared_ptr<ossia::scene_state> s = {})
      : state{std::move(s)}
  {
    output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Scene, {}});
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList&) const noexcept override
  {
    struct Renderer final : SourceRenderer
    {
      const StaticSceneNode& self;
      explicit Renderer(const StaticSceneNode& n)
          : SourceRenderer{n}
          , self{n}
      {
      }
      void init(score::gfx::RenderList&, QRhiResourceUpdateBatch&) override
      {
        m_initialized = true;
      }
      void update(
          score::gfx::RenderList&, QRhiResourceUpdateBatch&, score::gfx::Edge*) override
      {
        m_scene.state = self.state;
      }
    };
    return new Renderer{*this};
  }
};

}
