#pragma once

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>

namespace score::gfx
{

struct TexgenNode : NodeModel
{
  static const constexpr auto vertex = R"_(#version 450
  layout(location = 0) in vec2 position;
  layout(location = 1) in vec2 texcoord;

  layout(binding=3) uniform sampler2D y_tex;
  layout(location = 0) out vec2 v_texcoord;

  layout(std140, binding = 0) uniform renderer_t {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;
    vec2 renderSize;
  };

  out gl_PerVertex { vec4 gl_Position; };

  void main()
  {
    v_texcoord = texcoord;
    gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
  }
  )_";

  static const constexpr auto filter = R"_(#version 450
  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;

  layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
  };

  layout(binding=3) uniform sampler2D y_tex;


  void main ()
  {
    fragColor = texture(y_tex, v_texcoord);
  }
  )_";

  struct ubo
  {
    int currentImageIndex{};
    float pad;
    float position[2];
  } ubo;

#include <Gfx/Qt5CompatPush> // clang-format: keep
  struct Rendered : GenericNodeRenderer
  {
    using GenericNodeRenderer::GenericNodeRenderer;

    ~Rendered() { }

    QRhiTexture* texture{};
    void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override
    {
      const auto& mesh = renderer.defaultTriangle();
      defaultMeshInit(renderer, mesh, res);
      processUBOInit(renderer);
      m_material.init(renderer, node.input, m_samplers);
      std::tie(m_vertexS, m_fragmentS)
          = score::gfx::makeShaders(renderer.state, vertex, filter);

      auto& n = static_cast<const TexgenNode&>(this->node);
      auto& rhi = *renderer.state.rhi;
      {
        texture
            = rhi.newTexture(QRhiTexture::RGBA8, n.image.size(), 1, QRhiTexture::Flag{});

        texture->create();
      }

      {
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

        sampler->create();
        m_samplers.push_back({sampler, texture});
      }
      defaultPassesInit(renderer, mesh);
    }

    void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override
    {
      defaultUBOUpdate(renderer, res);
      auto& n = static_cast<const TexgenNode&>(this->node);
      if(func_t f = n.function.load())
      {
        f(const_cast<uchar*>(n.image.bits()), n.image.width(), n.image.height(), t++);
        res.uploadTexture(texture, n.image);
      }
    }

    void release(RenderList& r) override
    {
      texture->deleteLater();
      texture = nullptr;

      defaultRelease(r);
    }

    int t = 0;
  };

  QImage image;
  TexgenNode()
  {
    image = QImage{QSize(640, 480), QImage::Format_ARGB32_Premultiplied};
    output.push_back(new Port{this, {}, Types::Image, {}});
  }
  virtual ~TexgenNode()
  {
    m_materialData.release();
  }

  using func_t = void (*)(unsigned char* rgb, int width, int height, int t);
  std::atomic<func_t> function{};

  score::gfx::NodeRenderer* createRenderer(RenderList& r) const noexcept override
  {
    return new Rendered{*this};
  }
};

#include <Gfx/Qt5CompatPop> // clang-format: keep

}
