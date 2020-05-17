#pragma once

#include "node.hpp"
#include "renderer.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"

#include <Video/VideoDecoder.hpp>
using video_decoder = ::Video::VideoDecoder;
struct YUV420Node : NodeModel
{
  std::shared_ptr<video_decoder> decoder;

  // Taken from
  // https://www.roxlu.com/2014/039/decoding-h264-and-yuv420p-playback
  static const constexpr auto filter = R"_(#version 450

  layout(std140, binding = 0) uniform buf {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  } tbuf;

  layout(binding=3) uniform sampler2D y_tex;
  layout(binding=4) uniform sampler2D u_tex;
  layout(binding=5) uniform sampler2D v_tex;

  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;

  const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);
  const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);
  const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);
  const vec3 offset = vec3(-0.0625, -0.5, -0.5);

  void main ()
  {
    vec2 texcoord = vec2(v_texcoord.x, tbuf.texcoordAdjust.y + tbuf.texcoordAdjust.x * v_texcoord.y);

    float y = texture(y_tex, texcoord).r;
    float u = texture(u_tex, texcoord).r;
    float v = texture(v_tex, texcoord).r;
    vec3 yuv = vec3(y,u,v);
    yuv += offset;
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
    fragColor.r = dot(yuv, R_cf);
    fragColor.g = dot(yuv, G_cf);
    fragColor.b = dot(yuv, B_cf);
  })_";

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  YUV420Node(std::shared_ptr<video_decoder> dec)
      : decoder{std::move(dec)}
  {
    setShaders(m_mesh.defaultVertexShader(), filter);

    output.push_back(new Port{this, {}, Types::Image, {}});
  }

  const Mesh& mesh() const noexcept override { return this->m_mesh; }

  struct Rendered : RenderedNode
  {
    using RenderedNode::RenderedNode;
    QElapsedTimer t;
    std::vector<AVFrame*> framesToFree;

    ~Rendered()
    {
      auto& decoder = *static_cast<const YUV420Node&>(node).decoder;
      while (auto frame = decoder.dequeue_frame())
      {
        av_frame_free(&frame);
      }
    }

    void customInit(Renderer& renderer) override
    {
      auto& decoder = *static_cast<const YUV420Node&>(node).decoder;
      const auto w = decoder.width(), h = decoder.height();
      auto& rhi = *renderer.state.rhi;

      // Y
      {
        auto tex
            = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
        tex->build();

        auto sampler = rhi.newSampler(
            QRhiSampler::Linear,
            QRhiSampler::Linear,
            QRhiSampler::None,
            QRhiSampler::ClampToEdge,
            QRhiSampler::ClampToEdge);
        sampler->build();
        m_samplers.push_back({sampler, tex});
      }

      // U
      {
        auto tex = rhi.newTexture(
            QRhiTexture::R8, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
        tex->build();

        auto sampler = rhi.newSampler(
            QRhiSampler::Linear,
            QRhiSampler::Linear,
            QRhiSampler::None,
            QRhiSampler::ClampToEdge,
            QRhiSampler::ClampToEdge);
        sampler->build();
        m_samplers.push_back({sampler, tex});
      }

      // V
      {
        auto tex = rhi.newTexture(
            QRhiTexture::R8, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
        tex->build();

        auto sampler = rhi.newSampler(
            QRhiSampler::Linear,
            QRhiSampler::Linear,
            QRhiSampler::None,
            QRhiSampler::ClampToEdge,
            QRhiSampler::ClampToEdge);
        sampler->build();
        m_samplers.push_back({sampler, tex});
      }
    }

    void
    customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) override
    {
      for(auto frame : framesToFree)
        av_frame_free(&frame);
      framesToFree.clear();

      auto& decoder = *static_cast<const YUV420Node&>(node).decoder;
      if(!t.isValid() || t.elapsed() > (1000. / decoder.fps()))
      {
        if (auto frame = decoder.dequeue_frame())
        {
          setYPixels(res, frame->data[0], frame->linesize[0]);
          setUPixels(res, frame->data[1], frame->linesize[1]);
          setVPixels(res, frame->data[2], frame->linesize[2]);

          framesToFree.push_back(frame);
        }
        t.restart();
      }
    }

    void customRelease(Renderer&) override
    {
      for(auto [sampler, tex] : m_samplers)
        tex->releaseAndDestroyLater();
    }

    void setYPixels(
        QRhiResourceUpdateBatch& res,
        uint8_t* pixels,
        int stride) const noexcept
    {
      auto& decoder = *static_cast<const YUV420Node&>(node).decoder;
      // TODO glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
      const auto w = decoder.width(), h = decoder.height();
      auto y_tex = m_samplers[0].texture;
      QRhiTextureSubresourceUploadDescription subdesc;
      subdesc.setData(QByteArray::fromRawData(reinterpret_cast<const char*>(pixels), w * h));
      QRhiTextureUploadEntry entry{0, 0, subdesc};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(y_tex, desc);
    }

    void setUPixels(
        QRhiResourceUpdateBatch& res,
        uint8_t* pixels,
        int stride) const noexcept
    {
      auto& decoder = *static_cast<const YUV420Node&>(node).decoder;
      const auto w = decoder.width(), h = decoder.height();
      auto u_tex = m_samplers[1].texture;
      QRhiTextureSubresourceUploadDescription subdesc;
      subdesc.setData(QByteArray::fromRawData(reinterpret_cast<const char*>(pixels), w * h / 4));
      QRhiTextureUploadEntry entry{0, 0, subdesc};
      QRhiTextureUploadDescription desc{entry};

      res.uploadTexture(u_tex, desc);
    }

    void setVPixels(
        QRhiResourceUpdateBatch& res,
        uint8_t* pixels,
        int stride) const noexcept
    {
      auto& decoder = *static_cast<const YUV420Node&>(node).decoder;
      const auto w = decoder.width(), h = decoder.height();
      auto v_tex = m_samplers[2].texture;
      QRhiTextureSubresourceUploadDescription subdesc;
      subdesc.setData(QByteArray::fromRawData(reinterpret_cast<const char*>(pixels), w * h / 4));
      QRhiTextureUploadEntry entry{0, 0, subdesc};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(v_tex, desc);
    }
  };

  virtual ~YUV420Node() {}

  RenderedNode* createRenderer() const noexcept override
  {
    return new Rendered{*this};
  }
};

struct RGB0Node : NodeModel
{
  std::shared_ptr<video_decoder> decoder;

  static const constexpr auto filter = R"_(#version 450
  layout(std140, binding = 0) uniform buf {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  } tbuf;

  layout(binding=3) uniform sampler2D y_tex;

  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;

  void main ()
  {
    vec2 texcoord = vec2(v_texcoord.x, tbuf.texcoordAdjust.y + tbuf.texcoordAdjust.x * v_texcoord.y);

    fragColor = texture(y_tex, texcoord);
  })_";
  struct Rendered : RenderedNode
  {
    using RenderedNode::RenderedNode;
    QElapsedTimer t;
    std::vector<AVFrame*> framesToFree;

    ~Rendered()
    {
      auto& decoder = *static_cast<const RGB0Node&>(node).decoder;
      while (auto frame = decoder.dequeue_frame())
      {
        av_frame_free(&frame);
      }
    }

    void customInit(Renderer& renderer) override
    {
      auto& decoder = *static_cast<const RGB0Node&>(node).decoder;
      const auto w = decoder.width(), h = decoder.height();
      auto& rhi = *renderer.state.rhi;

      {
        auto tex = rhi.newTexture(
            QRhiTexture::RGBA8, QSize{w, h}, 1, QRhiTexture::Flag{});
        tex->build();

        auto sampler = rhi.newSampler(
            QRhiSampler::Linear,
            QRhiSampler::Linear,
            QRhiSampler::None,
            QRhiSampler::ClampToEdge,
            QRhiSampler::ClampToEdge);
        sampler->build();
        m_samplers.push_back({sampler, tex});
      }
    }

    void
    customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) override
    {
      for(auto frame : framesToFree)
        av_frame_free(&frame);
      framesToFree.clear();

      auto& decoder = *static_cast<const RGB0Node&>(node).decoder;
      if(!t.isValid() || t.elapsed() > (1000. / decoder.fps()))
      {
        if (auto frame = decoder.dequeue_frame())
        {
          setPixels(res, frame->data[0], frame->linesize[0]);

          framesToFree.push_back(frame);
        }
        t.restart();
      }
    }

    void customRelease(Renderer&) override
    {
      for(auto [sampler, tex] : m_samplers)
        tex->releaseAndDestroyLater();
    }

    void setPixels(
        QRhiResourceUpdateBatch& res,
        uint8_t* pixels,
        int stride) const noexcept
    {
      auto& decoder = *static_cast<const RGB0Node&>(node).decoder;
      const auto w = decoder.width(), h = decoder.height();
      auto y_tex = m_samplers[0].texture;
      QRhiTextureSubresourceUploadDescription subdesc;
      subdesc.setData(QByteArray::fromRawData(reinterpret_cast<const char*>(pixels), w * h * 4));
      QRhiTextureUploadEntry entry{0, 0, subdesc};
      QRhiTextureUploadDescription desc{entry};
      res.uploadTexture(y_tex, desc);
    }
/*
    std::optional<QSize> renderTargetSize() const noexcept override
    {
      auto& decoder = *static_cast<const RGB0Node&>(node).decoder;
      const auto w = decoder.width(), h = decoder.height();
      return QSize{w, h};
    }*/
  };

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  RGB0Node(std::shared_ptr<video_decoder> dec)
      : decoder{std::move(dec)}
  {
    setShaders(m_mesh.defaultVertexShader(), filter);
    output.push_back(new Port{this, {}, Types::Image, {}});
  }

  virtual ~RGB0Node() {}

  const Mesh& mesh() const noexcept override { return this->m_mesh; }
  RenderedNode* createRenderer() const noexcept override
  {
    return new Rendered{*this};
  }

};
