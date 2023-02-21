#pragma once

#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Scale.hpp>
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
/**
 * @brief Stores a sampler and the texture currently associated with it.
 */
struct Sampler
{
  QRhiSampler* sampler{};
  QRhiTexture* texture{};
};

/**
 * @brief Data model for audio data being sent to the GPU
 */
struct AudioTexture
{
  std::unordered_map<RenderList*, Sampler> samplers;

  std::vector<float> data;
  int channels{};
  int fixedSize{0};
  int rectUniformOffset{};
  bool fft{};
};

/**
 * @brief Port of a score::gfx::Node
 */
struct Port
{
  //! Parent node of the port
  score::gfx::Node* node{};

  //! Pointer to the corresponding data.
  void* value{};

  //! Type of the value
  Types type{};

  //! Edges connected to that port.
  std::vector<Edge*> edges;
};

/**
 * @brief Connection between two score::gfx::Port
 */
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
    if(auto it = std::find(source->edges.begin(), source->edges.end(), this);
       it != source->edges.end())
      source->edges.erase(it);
    if(auto it = std::find(sink->edges.begin(), sink->edges.end(), this);
       it != sink->edges.end())
      sink->edges.erase(it);
  }

  Port* source{};
  Port* sink{};
};

/**
 * @brief Useful abstraction for storing a graphics pipeline and associated resource bindings.
 */
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

/**
 * @brief Useful abstraction for storing all the data related to a render target.
 */
struct TextureRenderTarget
{
  QRhiTexture* texture{};
  QRhiRenderBuffer* colorRenderBuffer{};
  QRhiRenderBuffer* depthRenderBuffer{};
  QRhiRenderPassDescriptor* renderPass{};
  QRhiRenderTarget* renderTarget{};

  operator bool() const noexcept { return texture != nullptr; }

  void release()
  {
    if(texture)
    {
      delete texture;
      texture = nullptr;

      delete colorRenderBuffer;
      colorRenderBuffer = nullptr;

      delete depthRenderBuffer;
      depthRenderBuffer = nullptr;

      delete renderPass;
      renderPass = nullptr;

      delete renderTarget;
      renderTarget = nullptr;
    }
  }
};

/**
 * @brief Image data and metadata.
 */
struct Image
{
  QString path;
  std::vector<QImage> frames;
};

/**
 * @brief Create a render target from a texture.
 */
SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget
createRenderTarget(const RenderState& state, QRhiTexture* tex, int samples);

/**
 * @brief Create a render target from a texture format and size.
 *
 * This function will also create a texture.
 */
SCORE_PLUGIN_GFX_EXPORT
TextureRenderTarget createRenderTarget(
    const RenderState& state, QRhiTexture::Format fmt, QSize sz, int samples,
    QRhiTexture::Flags = {});

SCORE_PLUGIN_GFX_EXPORT
void replaceBuffer(QRhiShaderResourceBindings&, int binding, QRhiBuffer* newBuffer);
SCORE_PLUGIN_GFX_EXPORT
void replaceSampler(QRhiShaderResourceBindings&, int binding, QRhiSampler* newSampler);
SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(QRhiShaderResourceBindings&, int binding, QRhiTexture* newTexture);

SCORE_PLUGIN_GFX_EXPORT
void replaceBuffer(
    std::vector<QRhiShaderResourceBinding>&, int binding, QRhiBuffer* newBuffer);
SCORE_PLUGIN_GFX_EXPORT
void replaceSampler(
    std::vector<QRhiShaderResourceBinding>&, int binding, QRhiSampler* newSampler);
SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(
    std::vector<QRhiShaderResourceBinding>&, int binding, QRhiTexture* newTexture);

/**
 * @brief Replace a sampler.
 */
SCORE_PLUGIN_GFX_EXPORT
void replaceSampler(
    QRhiShaderResourceBindings&, QRhiSampler* oldSampler, QRhiSampler* newSampler);

/**
 * @brief Replace the texture currently bound to a sampler.
 */
SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(
    QRhiShaderResourceBindings&, QRhiSampler* sampler, QRhiTexture* newTexture);

/**
 * @brief Replace a texture by another in a set of bindings.
 */
SCORE_PLUGIN_GFX_EXPORT
void replaceTexture(
    QRhiShaderResourceBindings& srb, QRhiTexture* old_tex, QRhiTexture* new_tex);
/**
 * @brief Create bindings following the score conventions for shaders and materials.
 */
SCORE_PLUGIN_GFX_EXPORT
QRhiShaderResourceBindings* createDefaultBindings(
    const RenderList& renderer, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, const std::vector<Sampler>& samplers);

/**
 * @brief Create a render pipeline following the score conventions for shaders and materials.
 */
SCORE_PLUGIN_GFX_EXPORT
Pipeline buildPipeline(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, const std::vector<Sampler>& samplers);

/**
 * @brief Get a pair of compiled vertex / fragment shaders from GLSL 4.5 sources.
 *
 * Note: this function will throw if a shader is invalid.
 */
SCORE_PLUGIN_GFX_EXPORT
std::pair<QShader, QShader>
makeShaders(const RenderState& v, QString vert, QString frag);

/**
 * @brief Compile a compute shader.
 *
 * Note: this function will throw if the shader is invalid.
 */
SCORE_PLUGIN_GFX_EXPORT
QShader makeCompute(const RenderState& v, QString compt);

/**
 * @brief Utility to represent a shader material following score conventions.
 *
 * The material is synthesized from the input ports.
 */
struct SCORE_PLUGIN_GFX_EXPORT DefaultShaderMaterial
{
  void init(
      RenderList& renderer, const std::vector<Port*>& input,
      std::vector<Sampler>& samplers);

  QRhiBuffer* buffer{};
  int size{};
};

/**
 * @brief Resize the size of a texture to fit within GPU limits
 */
SCORE_PLUGIN_GFX_EXPORT
QSize resizeTextureSize(QSize img, int min, int max) noexcept;

/**
 * @brief Resize a texture to fit within GPU limits
 */
SCORE_PLUGIN_GFX_EXPORT
QImage resizeTexture(const QImage& img, int min, int max) noexcept;

inline void copyMatrix(const QMatrix4x4& mat, float* ptr) noexcept
{
  memcpy(ptr, mat.constData(), sizeof(float) * 16);
}
inline void copyMatrix(const QMatrix3x3& mat, float* ptr) noexcept
{
  memcpy(ptr, mat.constData(), sizeof(float) * 9);
}

/**
 * @brief Comput the scale to apply to a texture so that it fits in a GL viewport.
 */
SCORE_PLUGIN_GFX_EXPORT
QSizeF computeScale(score::gfx::ScaleMode mode, QSizeF viewport, QSizeF texture);
}
