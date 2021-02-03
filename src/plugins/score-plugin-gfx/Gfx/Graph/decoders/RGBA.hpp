#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

struct RGB0Decoder : GPUVideoDecoder
{
  static const constexpr auto rgb_filter = R"_(#version 450
    layout(std140, binding = 0) uniform buf {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;
    } tbuf;

    layout(binding=3) uniform sampler2D y_tex;

    layout(location = 0) in vec2 v_texcoord;
    layout(location = 0) out vec4 fragColor;

    vec4 processTexture(vec4 tex) {
      vec4 processed = tex;
      { %1 }
      return processed;
    }

    void main ()
    {
      vec2 texcoord = vec2(v_texcoord.x, tbuf.texcoordAdjust.y + tbuf.texcoordAdjust.x * v_texcoord.y);

      fragColor = processTexture(texture(y_tex, texcoord));
    })_";


  RGB0Decoder(QRhiTexture::Format fmt, NodeModel& n, video_decoder& d, QString f = "")
    : format{fmt}
    , node{n}
    , decoder{d}
    , filter{f}
  { }
  QRhiTexture::Format format;
  NodeModel& node;
  video_decoder& decoder;
  QString filter;

  void init(Renderer& r, RenderedNode& rendered) override
  {
    auto& rhi = *r.state.rhi;
    std::tie(node.m_vertexS, node.m_fragmentS) = makeShaders(node.mesh().defaultVertexShader(), QString(rgb_filter).arg(filter));

    const auto w = decoder.width, h = decoder.height;

    {
      auto tex = rhi.newTexture(format, QSize{w, h}, 1, QRhiTexture::Flag{});
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
    setPixels(rendered, res, frame.data[0], frame.linesize[0]);
  }

  void release(Renderer&, RenderedNode& n) override
  {
    for (auto [sampler, tex] : n.m_samplers)
      tex->releaseAndDestroyLater();
  }

  void setPixels(RenderedNode& rendered, QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width, h = decoder.height;
    auto y_tex = rendered.m_samplers[0].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 4, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(y_tex, desc);
  }
};
