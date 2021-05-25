#pragma once

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
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
struct GPUVideoDecoder
{
  virtual ~GPUVideoDecoder() { }
  virtual void init(RenderList& r, GenericNodeRenderer& rendered) = 0;
  virtual void exec(
      RenderList&,
      GenericNodeRenderer& rendered,
      QRhiResourceUpdateBatch& res,
      AVFrame& frame)
      = 0;
  virtual void release(RenderList&, GenericNodeRenderer& rendered) = 0;

  static inline QRhiTextureSubresourceUploadDescription createTextureUpload(
      uint8_t* pixels,
      int w,
      int h,
      int bytesPerPixel,
      int stride)
  {
    QRhiTextureSubresourceUploadDescription subdesc;

    const int rowBytes = w * bytesPerPixel;
    if (rowBytes == stride)
    {
      subdesc.setData(QByteArray::fromRawData(
          reinterpret_cast<const char*>(pixels), rowBytes * h));
    }
    else
    {
      QByteArray data{w * h, Qt::Uninitialized};
      for (int r = 0; r < h; r++)
      {
        const char* input = reinterpret_cast<const char*>(pixels + stride * r);
        char* output = data.data() + rowBytes * r;
        std::copy(input, input + rowBytes, output);
      }
      subdesc.setData(std::move(data));
    }

    return subdesc;
  }
};

struct EmptyDecoder : GPUVideoDecoder
{
  static const constexpr auto hashtag_no_filter = R"_(#version 450
    void main ()
    {
    }
  )_";

  EmptyDecoder(NodeModel& n)
      : node{n}
  {
  }

  NodeModel& node;
  void init(RenderList& r, GenericNodeRenderer& rendered) override
  {
    std::tie(node.m_vertexS, node.m_fragmentS) = score::gfx::makeShaders(
        node.mesh().defaultVertexShader(), hashtag_no_filter);
  }

  void exec(
      RenderList&,
      GenericNodeRenderer& rendered,
      QRhiResourceUpdateBatch& res,
      AVFrame& frame) override
  {
  }

  void release(RenderList&, GenericNodeRenderer& n) override { }
};
}
