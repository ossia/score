#pragma once

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Video/VideoInterface.hpp>

extern "C"
{
#include <libavutil/pixdesc.h>
}
namespace score::gfx
{
// TODO the "model" nodes should have a first update step so that they
// can share data across all renderers during a tick
struct VideoNode;
class GPUVideoDecoder
{
public:
  GPUVideoDecoder();
  virtual ~GPUVideoDecoder();
  [[nodiscard]]
  virtual std::pair<QShader, QShader> init(RenderList& r) = 0;
  virtual void exec(
      RenderList&,
      QRhiResourceUpdateBatch& res,
      AVFrame& frame)
      = 0;

  void release(RenderList&);

  static QRhiTextureSubresourceUploadDescription createTextureUpload(
      uint8_t* pixels,
      int w,
      int h,
      int bytesPerPixel,
      int stride);

  std::vector<Sampler> m_samplers;
};

struct EmptyDecoder : GPUVideoDecoder
{
  static const constexpr auto hashtag_no_filter = R"_(#version 450
    void main ()
    {
    }
  )_";

  explicit EmptyDecoder()
  {
  }

  std::pair<QShader, QShader> init(RenderList& r) override
  {
    return score::gfx::makeShaders(TexturedTriangle::instance().defaultVertexShader(), hashtag_no_filter);
  }

  void exec(
      RenderList&,
      QRhiResourceUpdateBatch& res,
      AVFrame& frame) override
  {
  }
};
}
