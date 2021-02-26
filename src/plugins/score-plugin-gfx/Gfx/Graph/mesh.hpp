#pragma once
#include <ossia/detail/small_vector.hpp>

#include <private/qrhi_p.h>

#include <ossia/detail/span.hpp>
#include <score_plugin_gfx_export.h>

struct SCORE_PLUGIN_GFX_EXPORT Mesh
{
  ossia::small_vector<QRhiVertexInputBinding, 2> vertexInputBindings;
  ossia::small_vector<QRhiVertexInputAttribute, 2> vertexAttributeBindings;
  int vertexCount{};
  int indexCount{};
  gsl::span<const float> vertexArray;
  gsl::span<const unsigned int> indexArray;

  Mesh();
  virtual ~Mesh();
  virtual void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb)
      const noexcept = 0;
  virtual const char* defaultVertexShader() const noexcept = 0;

private:
  Mesh(const Mesh&) = delete;
  Mesh(Mesh&&) = delete;
  Mesh& operator=(const Mesh&) = delete;
  Mesh& operator=(Mesh&&) = delete;
};

struct PlainMesh : Mesh
{
  PlainMesh(gsl::span<const float> vtx, int count)
  {
    vertexInputBindings.push_back({2 * sizeof(float)});
    vertexAttributeBindings.push_back({0, 0, QRhiVertexInputAttribute::Float2, 0});
    vertexArray = vtx;
    vertexCount = count;
  }

  void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb)
      const noexcept override
  {
    const QRhiCommandBuffer::VertexInput bindings[] = {{&vtxData, 0}};

    cb.setVertexInput(0, 1, bindings, idxData);
  }

  const char* defaultVertexShader() const noexcept override
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
};

struct TexturedMesh : Mesh
{
  TexturedMesh(gsl::span<const float> vtx, int count)
  {
    vertexInputBindings.push_back({2 * sizeof(float)});
    vertexInputBindings.push_back({2 * sizeof(float)});
    vertexAttributeBindings.push_back({0, 0, QRhiVertexInputAttribute::Float2, 0});
    vertexAttributeBindings.push_back({1, 1, QRhiVertexInputAttribute::Float2, 0});
    vertexArray = vtx;
    vertexCount = count;
  }

  void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb)
      const noexcept override
  {
    const QRhiCommandBuffer::VertexInput bindings[]
        = {{&vtxData, 0}, {&vtxData, 3 * 2 * sizeof(float)}};

    cb.setVertexInput(0, 2, bindings);
  }

  const char* defaultVertexShader() const noexcept override
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
};

struct TextureNormalMesh : Mesh
{
  TextureNormalMesh(gsl::span<const float> vtx, gsl::span<const unsigned int> idx, int count)
  {
    vertexInputBindings.push_back({8 * sizeof(float)});
    // int binding, int location, Format format, quint32 offset
    vertexAttributeBindings.push_back({0, 0, QRhiVertexInputAttribute::Float3, 0});
    vertexAttributeBindings.push_back({0, 1, QRhiVertexInputAttribute::Float3, 3 * sizeof(float)});
    vertexAttributeBindings.push_back({0, 2, QRhiVertexInputAttribute::Float2, 6 * sizeof(float)});

    vertexArray = vtx;
    indexArray = idx;
    vertexCount = count;
  }

  void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb)
      const noexcept override
  {
    const QRhiCommandBuffer::VertexInput bindings[] = {{&vtxData, 0}};

    cb.setVertexInput(0, 1, bindings, idxData, 0, QRhiCommandBuffer::IndexUInt32);
  }

  const char* defaultVertexShader() const noexcept override
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
};

struct PlainTriangle final : PlainMesh
{
  static const constexpr float data[] = {-1, -1, 3, -1, -1, 3};

  PlainTriangle() : PlainMesh{data, 3} { }

  static const PlainTriangle& instance() noexcept
  {
    static const PlainTriangle t;
    return t;
  }
};

struct TexturedTriangle final : TexturedMesh
{
  static const constexpr float data[] = {
      -1,
      -1,
      3,
      -1,
      -1,
      3,
      (0 / 2) * 2.,
      (1. - (0 % 2) * 2.),
      (2 / 2) * 2.,
      (1. - (2 % 2) * 2.),
      (1 / 2) * 2.,
      (1. - (1 % 2) * 2.),
  };

  TexturedTriangle() : TexturedMesh{data, 3} { }

  static const TexturedTriangle& instance() noexcept
  {
    static const TexturedTriangle t;
    return t;
  }
};
