#pragma once
#include "mesh.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"
#include "utils.hpp"

#include <ossia/detail/flat_map.hpp>
#include <ossia/detail/packed_struct.hpp>

#include <score_plugin_gfx_export.h>

#include <algorithm>
#include <optional>
#include <vector>

#include <unordered_map>

packed_struct ProcessUBO
{
  float time{};
  float timeDelta{};
  float progress{};

  int32_t passIndex{};
  int32_t frameIndex{};

  float date[4]{0.f, 0.f, 0.f, 0.f};
  float mouse[4]{0.5f, 0.5f, 0.5f, 0.5f};
  float channelTime[4]{0.5f, 0.5f, 0.5f, 0.5f};

  float sampleRate{};
};
end_packed_struct

    packed_struct ModelCameraUBO
{
  float mvp[16]{};
  float mv[16]{};
  float model[16]{};
  float view[16]{};
  float projection[16]{};
  float modelNormal[9]{};
};
end_packed_struct

    static_assert(
        sizeof(ModelCameraUBO)
        == sizeof(float) * (16 + 16 + 16 + 16 + 16 + 9));
struct Renderer;
class RenderedNode;

namespace score::gfx
{
class NodeRenderer;
class SCORE_PLUGIN_GFX_EXPORT Node
{
  friend class NodeRenderer;

public:
  explicit Node() { }

  virtual ~Node() { }

  virtual NodeRenderer* createRenderer(Renderer& r) const noexcept = 0;
  virtual const Mesh& mesh() const noexcept = 0;

  std::vector<Port*> input;
  std::vector<Port*> output;
  ossia::flat_map<Renderer*, score::gfx::NodeRenderer*> renderedNodes;

  bool addedToGraph{};
};

class SCORE_PLUGIN_GFX_EXPORT ProcessNode : public Node
{
public:
  using Node::Node;

  int64_t materialChanged{0};
  ProcessUBO standardUBO{};
};

class SCORE_PLUGIN_GFX_EXPORT NodeRenderer
{
public:
  explicit NodeRenderer() noexcept;
  virtual ~NodeRenderer();

  virtual std::optional<QSize> renderTargetSize() const noexcept = 0;
  virtual TextureRenderTarget renderTarget() const noexcept = 0;
  virtual void init(Renderer& renderer) = 0;
  virtual void update(Renderer& renderer, QRhiResourceUpdateBatch& res) = 0;
  virtual void runPass(
      Renderer&,
      QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch& updateBatch)
      = 0;
  virtual void release(Renderer&) = 0;
  virtual void releaseWithoutRenderTarget(Renderer&) = 0;
};

}
class SCORE_PLUGIN_GFX_EXPORT RenderedNode : public score::gfx::NodeRenderer
{
public:
  RenderedNode(const NodeModel& node) noexcept
      : NodeRenderer{}
      , node{node}
  {
  }

  virtual ~RenderedNode() { }

  const NodeModel& node;

  TextureRenderTarget m_rt;

  std::vector<Sampler> m_samplers;

  // Pipeline
  Pipeline m_p;

  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_processUBO{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};
  int64_t materialChangedIndex{-1};

  friend struct Graph;
  friend struct Renderer;

  TextureRenderTarget createRenderTarget(const RenderState& state);
  TextureRenderTarget renderTarget() const noexcept override { return m_rt; }

  std::optional<QSize> renderTargetSize() const noexcept override;
  // Render loop
  virtual void customInit(Renderer& renderer);
  void init(Renderer& renderer) override;

  virtual void customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res);
  void update(Renderer& renderer, QRhiResourceUpdateBatch& res) override;

  virtual void customRelease(Renderer&);
  void release(Renderer&) override;
  void releaseWithoutRenderTarget(Renderer&) override;

  void runPass(
      Renderer&,
      QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch& updateBatch) override;

  void defaultShaderMaterialInit(Renderer& renderer);
  QRhiGraphicsPipeline* pipeline() const { return m_p.pipeline; }
  QRhiShaderResourceBindings* resources() const { return m_p.srb; }
};

class SCORE_PLUGIN_GFX_EXPORT NodeModel : public score::gfx::ProcessNode
{
  friend class RenderedNode;

public:
  explicit NodeModel();
  virtual ~NodeModel();

  virtual score::gfx::NodeRenderer* createRenderer(Renderer& r) const noexcept;

  void setShaders(const QShader& vert, const QShader& frag);

  std::unique_ptr<char[]> m_materialData;

  QShader m_vertexS;
  QShader m_fragmentS;

  friend class RenderedNode;
};
