#pragma once

#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>

#include <score_plugin_gfx_export.h>

#include <unordered_map>

namespace score::gfx
{
class Node;
class NodeModel;
struct Port;
struct Edge;
class RenderList;

struct Sampler
{
  QRhiSampler* sampler{};
  QRhiTexture* texture{};
};

struct AudioTexture
{
  std::unordered_map<RenderList*, Sampler> samplers;

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
  Edge(Port* source, Port* sink)
      : source{source}
      , sink{sink}
  {
    source->edges.push_back(this);
    sink->edges.push_back(this);
  }

  ~Edge()
  {
    if (auto it = std::find(source->edges.begin(), source->edges.end(), this);
        it != source->edges.end())
      source->edges.erase(it);
    if (auto it = std::find(sink->edges.begin(), sink->edges.end(), this);
        it != sink->edges.end())
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

struct TextureRenderTarget
{
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

struct Image
{
  QString path;
  std::vector<QImage> frames;
};

SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget
createRenderTarget(const RenderState& state, QRhiTexture* tex);

SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget createRenderTarget(
    const RenderState& state,
    QRhiTexture::Format fmt,
    QSize sz);

SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(
    QRhiShaderResourceBindings&,
    QRhiSampler* sampler,
    QRhiTexture* newTexture);

SCORE_PLUGIN_GFX_EXPORT
QRhiShaderResourceBindings* createDefaultBindings(
    const RenderList& renderer,
    const TextureRenderTarget& rt,
    QRhiBuffer* m_processUBO,
    QRhiBuffer* materialUBO,
    const std::vector<Sampler>& samplers);

SCORE_PLUGIN_GFX_EXPORT
Pipeline buildPipeline(
    const RenderList& renderer,
    const Mesh& mesh,
    const QShader& vertexS,
    const QShader& fragmentS,
    const TextureRenderTarget& rt,
    QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO,
    const std::vector<Sampler>& samplers);

SCORE_PLUGIN_GFX_EXPORT
std::pair<QShader, QShader> makeShaders(QString vert, QString frag);

SCORE_PLUGIN_GFX_EXPORT
QShader makeCompute(QString compt);

SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(
    QRhiShaderResourceBindings& srb,
    QRhiTexture* old_tex,
    QRhiTexture* new_tex);

struct SCORE_PLUGIN_GFX_EXPORT DefaultShaderMaterial
{
  void init(
      RenderList& renderer,
      const std::vector<Port*>& input,
      std::vector<Sampler>& m_samplers);

  QRhiBuffer* buffer{};
  int size{};
};


}
