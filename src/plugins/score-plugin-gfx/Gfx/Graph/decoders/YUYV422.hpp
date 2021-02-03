#pragma once
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>

struct YUYV422Decoder : GPUVideoDecoder
{
static const constexpr auto filter = R"_(#version 450

layout(std140, binding = 0) uniform buf {
mat4 clipSpaceCorrMatrix;
vec2 texcoordAdjust;
} tbuf;

layout(binding=3) uniform sampler2D u_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

// See https://gist.github.com/roxlu/7872352
const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);
const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);
const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);
const vec3 offset = vec3(-0.0625, -0.5, -0.5);

void main() {
  vec3 tc =  texture(u_tex, v_texcoord).rgb;
  vec3 yuv = vec3(tc.g, tc.b, tc.r);
  yuv += offset;
  fragColor.r = dot(yuv, R_cf);
  fragColor.g = dot(yuv, G_cf);
  fragColor.b = dot(yuv, B_cf);
  fragColor.a = 1.0;
}
)_";

  YUYV422Decoder(NodeModel& n, video_decoder& d): node{n}, decoder{d} { }
  NodeModel& node;
  video_decoder& decoder;
  void init(Renderer& r, RenderedNode& rendered) override
  {
    auto& rhi = *r.state.rhi;

    std::tie(node.m_vertexS, node.m_fragmentS) = makeShaders(node.mesh().defaultVertexShader(), filter);

    const auto w = decoder.width, h = decoder.height;
    // Y
    {
      auto tex = rhi.newTexture(QRhiTexture::RGBA8, {w/4, h/2}, 1, QRhiTexture::Flag{});
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

};
