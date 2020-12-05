#pragma once
#include "mesh.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"

#include <ossia/detail/flat_map.hpp>

#include <QShaderBaker>

#include <algorithm>
#include <optional>
#include <vector>

#include <unordered_map>
#include <score_plugin_gfx_export.h>

namespace score::gfx
{
class Node;
}
class NodeModel;
struct Port;
struct Edge;
struct Renderer;
struct AudioTexture
{
  std::unordered_map<Renderer*, std::pair<QRhiSampler*, QRhiTexture*>> samplers;

  std::vector<float> data;
  int channels{};
  int fixedSize{0};
  bool fft{};
};

struct Port
{
  score::gfx::Node* node{};
  void* value{};
  Types type{};
  std::vector<Edge*> edges;
};

struct Edge
{
  Edge(Port* source, Port* sink) : source{source}, sink{sink}
  {
    source->edges.push_back(this);
    sink->edges.push_back(this);
  }

  ~Edge()
  {
    if (auto it = std::find(source->edges.begin(), source->edges.end(), this);
        it != source->edges.end())
      source->edges.erase(it);
    if (auto it = std::find(sink->edges.begin(), sink->edges.end(), this); it != sink->edges.end())
      sink->edges.erase(it);
  }

  Port* source{};
  Port* sink{};
};

struct Pipeline
{
  QRhiGraphicsPipeline* pipeline{};
  QRhiShaderResourceBindings* srb{};

  void release()
  {
    delete pipeline;
    pipeline = nullptr;

    delete srb;
    srb = nullptr;
  }
};

struct Sampler
{
  QRhiSampler* sampler{};
  QRhiTexture* texture{};
};

struct TextureRenderTarget {
  QRhiTexture* texture{};
  QRhiRenderPassDescriptor* renderPass{};
  QRhiRenderTarget* renderTarget{};

  operator bool() const noexcept { return texture != nullptr; }

  void release()
  {
    if (texture)
    {
      delete texture;
      texture = nullptr;

      delete renderPass;
      renderPass = nullptr;

      delete renderTarget;
      renderTarget = nullptr;
    }
  }
};

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    ProcessUBO
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
#if defined(_MSC_VER)
#pragma pack()
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    ModelCameraUBO
{
  float mvp[16]{};
  float mv[16]{};
  float model[16]{};
  float view[16]{};
  float projection[16]{};
  float modelNormal[9]{};
};
#if defined(_MSC_VER)
#pragma pack()
#endif
static_assert(sizeof(ModelCameraUBO) == sizeof(float) * (16 + 16 + 16 + 16 + 16 + 9));
struct Renderer;
class RenderedNode;

namespace score::gfx
{
class NodeRenderer;
class SCORE_PLUGIN_GFX_EXPORT Node
{
  friend class NodeRenderer;

public:
  explicit Node()
  {

  }

  virtual ~Node()
  {

  }

  virtual NodeRenderer* createRenderer() const noexcept = 0;
  virtual const Mesh& mesh() const noexcept = 0;

  std::vector<Port*> input;
  std::vector<Port*> output;
  ossia::flat_map<Renderer*, score::gfx::NodeRenderer*> renderedNodes;

  bool addedToGraph{};
};

class SCORE_PLUGIN_GFX_EXPORT ProcessNode
    : public Node
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
  virtual TextureRenderTarget createRenderTarget(const RenderState& state) = 0;
  virtual void init(Renderer& renderer) = 0;
  virtual void update(Renderer& renderer, QRhiResourceUpdateBatch& res) = 0;
  virtual void runPass(Renderer&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch& updateBatch) = 0;
  virtual void release(Renderer&) = 0;
  virtual void releaseWithoutRenderTarget(Renderer&) = 0;
};

SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget createRenderTarget(const RenderState& state, QSize sz);

SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(QRhiShaderResourceBindings&, QRhiSampler* sampler, QRhiTexture* newTexture);

SCORE_PLUGIN_GFX_EXPORT
Pipeline buildPipeline(
      const Renderer& renderer,
      const Mesh& mesh,
      const QShader& vertexS, const QShader& fragmentS,
      const TextureRenderTarget& rt,
      QRhiBuffer* processUBO,
      QRhiBuffer* materialUBO,
      const std::vector<Sampler>& samplers);
}

class SCORE_PLUGIN_GFX_EXPORT RenderedNode
    : public score::gfx::NodeRenderer
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

  virtual TextureRenderTarget createRenderTarget(const RenderState& state);

  virtual std::optional<QSize> renderTargetSize() const noexcept;
  // Render loop
  virtual void customInit(Renderer& renderer);
  void init(Renderer& renderer);

  virtual void customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res);
  void update(Renderer& renderer, QRhiResourceUpdateBatch& res);

  virtual void customRelease(Renderer&);
  void release(Renderer&);
  void releaseWithoutRenderTarget(Renderer&);

  virtual void runPass(Renderer&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch& updateBatch);


  void defaultShaderMaterialInit(Renderer& renderer);
  QRhiGraphicsPipeline* pipeline() const { return m_p.pipeline; }
  QRhiShaderResourceBindings* resources() const { return m_p.srb; }
};

class SCORE_PLUGIN_GFX_EXPORT NodeModel
    : public score::gfx::ProcessNode
{
  friend class RenderedNode;

public:
  explicit NodeModel();
  virtual ~NodeModel();

  virtual score::gfx::NodeRenderer* createRenderer() const noexcept;

  void setShaders(QString vert, QString frag);
  void setShaders(const QShader& vert, const QShader& frag);

  std::unique_ptr<char[]> m_materialData;

protected:
  QShader m_vertexS;
  QShader m_fragmentS;

  friend class RenderedNode;
};
