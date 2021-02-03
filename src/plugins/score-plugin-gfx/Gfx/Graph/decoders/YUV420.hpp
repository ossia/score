#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>


struct YUV420Decoder : GPUVideoDecoder
{
  // Taken from
  // https://www.roxlu.com/2014/039/decoding-h264-and-yuv420p-playback
static const constexpr auto yuv420_filter = R"_(#version 450

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


  YUV420Decoder(NodeModel& n, video_decoder& d)
    : node{n}
    , decoder{d}
  {

  }

  NodeModel& node;
  video_decoder& decoder;

  void init(Renderer& r, RenderedNode& rendered) override
  {
    auto& rhi = *r.state.rhi;
    std::tie(node.m_vertexS, node.m_fragmentS) = makeShaders(node.mesh().defaultVertexShader(), yuv420_filter);
    const auto w = decoder.width, h = decoder.height;

    // Y
    {
      auto tex = rhi.newTexture(QRhiTexture::R8, {w, h}, 1, QRhiTexture::Flag{});
      tex->build();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->build();
      rendered.m_samplers.push_back({sampler, tex});
    }

    // U
    {
      auto tex = rhi.newTexture(QRhiTexture::R8, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
      tex->build();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->build();
      rendered.m_samplers.push_back({sampler, tex});
    }

    // V
    {
      auto tex = rhi.newTexture(QRhiTexture::R8, {w / 2, h / 2}, 1, QRhiTexture::Flag{});
      tex->build();

      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->build();
      rendered.m_samplers.push_back({sampler, tex});
    }
  }

  void exec(Renderer&, RenderedNode& rendered, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
    setYPixels(rendered, res, frame.data[0], frame.linesize[0]);
    setUPixels(rendered, res, frame.data[1], frame.linesize[1]);
    setVPixels(rendered, res, frame.data[2], frame.linesize[2]);
  }

  void release(Renderer&, RenderedNode& n) override
  {
    for (auto [sampler, tex] : n.m_samplers)
      tex->releaseAndDestroyLater();
  }

  void setYPixels(RenderedNode& rendered, QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = rendered.m_samplers[0].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 1, stride)};
    QRhiTextureUploadDescription desc{entry};

    res.uploadTexture(y_tex, desc);
  }

  void setUPixels(RenderedNode& rendered, QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width / 2, h = decoder.height /2;
    auto u_tex = rendered.m_samplers[1].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 1, stride)};
    QRhiTextureUploadDescription desc{entry};

    res.uploadTexture(u_tex, desc);
  }

  void setVPixels(RenderedNode& rendered, QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width / 2, h = decoder.height /2;
    auto v_tex = rendered.m_samplers[2].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 1, stride)};
    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(v_tex, desc);
  }
};
