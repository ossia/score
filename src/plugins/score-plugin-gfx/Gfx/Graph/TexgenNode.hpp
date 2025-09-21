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

  layout(binding = 3) uniform sampler2D y_tex;
  layout(location = 0) out vec2 v_texcoord;

  layout(std140, binding = 0) uniform renderer_t {
    mat4 clipSpaceCorrMatrix;
    vec2 renderSize;
  } renderer;

  out gl_PerVertex { vec4 gl_Position; };

  void main()
  {
    v_texcoord = texcoord;
    gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
#if !(defined(QSHADER_SPIRV) || defined(QSHADER_HLSL) || defined(QSHADER_MSL))
  gl_Position.y = - gl_Position.y;
#endif
  }
  )_";

  static const constexpr auto filter = R"_(#version 450
  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;

  layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
  } renderer;

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

      auto& rhi = *renderer.state.rhi;
      {
        vec.resize(640 * 480 * 4);
        texture = rhi.newTexture(
            QRhiTexture::RGBA8, QSize(640, 480), 1, QRhiTexture::Flag{});

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

    void update(
        RenderList& renderer, QRhiResourceUpdateBatch& res,
        score::gfx::Edge* edge) override
    {
      if(!edge)
        return;

      auto& n = const_cast<TexgenNode&>(static_cast<const TexgenNode&>(this->node));
      defaultUBOUpdate(renderer, res);
      auto sz = renderer.renderSize(edge);
      auto bytes = sz.width() * sz.height() * 4;
      if(bytes != vec.size())
      {
        vec.resize(bytes, boost::container::default_init);

        QRhiTexture* oldtex = texture;
        QRhiTexture* newtex = renderer.state.rhi->newTexture(
            QRhiTexture::RGBA8, sz, 1, QRhiTexture::Flag{});
        newtex->create();
        for(auto& [edge, pass] : this->m_p)
          if(pass.srb)
            score::gfx::replaceTexture(*pass.srb, m_samplers[0].sampler, newtex);
        texture = newtex;

        if(oldtex && oldtex != &renderer.emptyTexture())
        {
          oldtex->deleteLater();
        }
      }

      if(func_t f = n.function.load())
      {
        f(vec.data(), sz.width(), sz.height(), t++);

        QRhiTextureSubresourceUploadDescription subdesc{
            QByteArray::fromRawData((const char*)vec.data(), vec.size())};
        QRhiTextureUploadEntry entry{0, 0, subdesc};
        QRhiTextureUploadDescription desc{entry};

        res.uploadTexture(texture, desc);
      }
    }

    void release(RenderList& r) override
    {
      texture->deleteLater();
      texture = nullptr;

      defaultRelease(r);
    }

    int t = 0;
    boost::container::vector<unsigned char> vec;
  };

  TexgenNode() { output.push_back(new Port{this, {}, Types::Image, {}}); }
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


}
