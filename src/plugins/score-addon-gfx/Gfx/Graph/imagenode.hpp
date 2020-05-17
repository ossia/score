#pragma once

#include "node.hpp"
#include "renderer.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"

namespace Gfx
{
struct Image
{
  QString path;
  QImage image;
};
}

struct ImagesNode : NodeModel
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
  layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
  };

  layout(std140, binding = 2) uniform material_t {
    int idx;
    vec2 position;
  };

  layout(binding=3) uniform sampler2D y_tex;

  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;

  void main ()
  {
    vec2 factor = textureSize(y_tex, 0) / renderSize;
    vec2 ifactor = renderSize / textureSize(y_tex, 0);
    vec2 texcoord = vec2(v_texcoord.x, texcoordAdjust.y + texcoordAdjust.x * v_texcoord.y);
    fragColor = texture(y_tex, vec2(1) - ifactor * position + texcoord / factor);
  }
  )_";

  struct ubo {
  int currentImageIndex{}; float pad;
  float position[2];
  } ubo;

  struct Rendered : RenderedNode
  {
    using RenderedNode::RenderedNode;

    bool m_uploaded = false;
    ~Rendered()
    {
    }

    std::vector<QRhiTexture*> textures;
    void customInit(Renderer& renderer) override
    {
      prev_ubo.currentImageIndex = -1;
      auto& n = static_cast<const ImagesNode&>(this->node);
      auto& rhi = *renderer.state.rhi;
      for(const Gfx::Image& img: n.images)
      {
        const QSize sz = img.image.size();
        auto tex = rhi.newTexture(
            QRhiTexture::BGRA8, QSize{sz.width(), sz.height()}, 1, QRhiTexture::Flag{});

        tex->build();
        textures.push_back(tex);
      }

      {
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear,
            QRhiSampler::Linear,
            QRhiSampler::None,
            QRhiSampler::ClampToEdge,
            QRhiSampler::ClampToEdge);

        sampler->build();
        m_samplers.push_back({sampler, renderer.m_emptyTexture});
      }
    }

    void
    customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) override
    {
      if(textures.empty())
        return;

      auto& n = static_cast<const ImagesNode&>(this->node);
      if(!m_uploaded)
      {
        for(int i = 0; i < n.images.size(); i++)
        {
          res.uploadTexture(textures[i], n.images[i].image);
        }
        m_uploaded = true;
      }

      if(prev_ubo.currentImageIndex != n.ubo.currentImageIndex)
      {
        replaceTexture(m_samplers[0].sampler, textures[n.ubo.currentImageIndex]);
        prev_ubo.currentImageIndex = n.ubo.currentImageIndex;
      }

    }

    void customRelease(Renderer&) override
    {
      for(auto tex : textures)
        tex->releaseAndDestroyLater();
      textures.clear();
    }

    struct ubo prev_ubo;
    /*
    std::optional<QSize> renderTargetSize() const noexcept override
    {
      auto& decoder = *static_cast<const RGB0Node&>(node).decoder;
      const auto w = decoder.width(), h = decoder.height();
      return QSize{w, h};
    }*/
  };

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  std::vector<Gfx::Image> images;
  ImagesNode(std::vector<Gfx::Image> dec)
      : images{std::move(dec)}
  {
    setShaders(vertex, filter);
    input.push_back(new Port{this, &ubo.currentImageIndex, Types::Int, {}});
    input.push_back(new Port{this, &ubo.position[0], Types::Vec2, {}});
    output.push_back(new Port{this, {}, Types::Image, {}});

    m_materialData.reset((char*)&ubo);
  }
  virtual ~ImagesNode() { m_materialData.release(); }

  const Mesh& mesh() const noexcept override { return this->m_mesh; }
  RenderedNode* createRenderer() const noexcept override
  {
    return new Rendered{*this};
  }
};
