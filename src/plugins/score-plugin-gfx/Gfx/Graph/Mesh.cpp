#include <Gfx/Graph/Mesh.hpp>

namespace score::gfx
{

Mesh::Mesh() { }

Mesh::~Mesh() { }

PlainMesh::PlainMesh(gsl::span<const float> vtx, int count)
{
  vertexInputBindings.push_back({2 * sizeof(float)});
  vertexAttributeBindings.push_back(
      {0, 0, QRhiVertexInputAttribute::Float2, 0});
  vertexArray = vtx;
  vertexCount = count;
}

void PlainMesh::setupBindings(
    QRhiBuffer& vtxData,
    QRhiBuffer* idxData,
    QRhiCommandBuffer& cb) const noexcept
{
  const QRhiCommandBuffer::VertexInput bindings[] = {{&vtxData, 0}};

  cb.setVertexInput(0, 1, bindings, idxData);
}

const char* PlainMesh::defaultVertexShader() const noexcept
{
  return R"_(#version 450
layout(location = 0) in vec2 position;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";
}

TexturedMesh::TexturedMesh(gsl::span<const float> vtx, int count)
{
  vertexInputBindings.push_back({2 * sizeof(float)});
  vertexInputBindings.push_back({2 * sizeof(float)});
  vertexAttributeBindings.push_back(
      {0, 0, QRhiVertexInputAttribute::Float2, 0});
  vertexAttributeBindings.push_back(
      {1, 1, QRhiVertexInputAttribute::Float2, 0});
  vertexArray = vtx;
  vertexCount = count;
}

const char* TexturedMesh::defaultVertexShader() const noexcept
{
  return R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";
}

TextureNormalMesh::TextureNormalMesh(
    gsl::span<const float> vtx,
    gsl::span<const unsigned int> idx,
    int count)
{
  vertexInputBindings.push_back({8 * sizeof(float)});
  // int binding, int location, Format format, quint32 offset
  vertexAttributeBindings.push_back(
      {0, 0, QRhiVertexInputAttribute::Float3, 0});
  vertexAttributeBindings.push_back(
      {0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)});
  vertexAttributeBindings.push_back(
      {0, 2, QRhiVertexInputAttribute::Float2, 6 * sizeof(float)});

  vertexArray = vtx;
  indexArray = idx;
  vertexCount = count;
}

void TextureNormalMesh::setupBindings(
    QRhiBuffer& vtxData,
    QRhiBuffer* idxData,
    QRhiCommandBuffer& cb) const noexcept
{
  const QRhiCommandBuffer::VertexInput bindings[] = {{&vtxData, 0}};

  cb.setVertexInput(
      0, 1, bindings, idxData, 0, QRhiCommandBuffer::IndexUInt32);
}

const char* TextureNormalMesh::defaultVertexShader() const noexcept
{
  return R"_(#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexTexCoord;

layout(location = 0) out vec3 esVertex;
layout(location = 1) out vec3 esNormal;
layout(location = 2) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
mat4 clipSpaceCorrMatrix;
vec2 texcoordAdjust;
vec2 renderSize;
};

layout(std140, binding = 1) uniform process_t {
  float time;
  float timeDelta;
  float progress;

  int passIndex;
  int frameIndex;

  vec4 date;
  vec4 mouse;
  vec4 channelTime;

  float sampleRate;
};
layout(std140, binding = 2) uniform model_t {
mat4 matrixModelViewProjection;
mat4 matrixModelView;
mat4 matrixModel;
mat4 matrixView;
mat4 matrixProjection;
mat3 matrixNormal;
};

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    esVertex = vec3(matrixModelView * vec4(position, 1.0));
    esNormal = matrixNormal * vec3(vertexNormal);
    v_texcoord = vertexTexCoord;
    gl_Position = clipSpaceCorrMatrix * matrixModelViewProjection * vec4(position, 1.0);
}
)_";
}

PlainTriangle::PlainTriangle()
    : PlainMesh{data, 3}
{
}

const PlainTriangle& PlainTriangle::instance() noexcept
{
  static const PlainTriangle t;
  return t;
}

TexturedTriangle::TexturedTriangle()
    : TexturedMesh{data, 3}
{
}

const TexturedTriangle& TexturedTriangle::instance() noexcept
{
  static const TexturedTriangle t;
  return t;
}


void TexturedTriangle::setupBindings(
    QRhiBuffer& vtxData,
    QRhiBuffer* idxData,
    QRhiCommandBuffer& cb) const noexcept
{
  const QRhiCommandBuffer::VertexInput bindings[]
      = {{&vtxData, 0},
         {&vtxData, 3 * 2 * sizeof(float)}};

  cb.setVertexInput(0, 2, bindings);
}

TexturedQuad::TexturedQuad()
    : TexturedMesh{data, 4}
{
}

const TexturedQuad& TexturedQuad::instance() noexcept
{
  static const TexturedQuad t;
  return t;
}

void TexturedQuad::setupBindings(
    QRhiBuffer& vtxData,
    QRhiBuffer* idxData,
    QRhiCommandBuffer& cb) const noexcept
{
  const QRhiCommandBuffer::VertexInput bindings[]
      = {{&vtxData, 0},
         {&vtxData, 4 * 2 * sizeof(float)}};

  cb.setVertexInput(0, 2, bindings);
}
}
