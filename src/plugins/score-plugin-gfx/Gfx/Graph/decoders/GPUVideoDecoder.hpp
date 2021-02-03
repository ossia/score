#pragma once

#include <Gfx/Graph/node.hpp>
#include <Gfx/Graph/renderer.hpp>
#include <Gfx/Graph/renderstate.hpp>
#include <Gfx/Graph/uniforms.hpp>

#include <Video/VideoInterface.hpp>

extern "C"
{
#include <libavutil/pixdesc.h>
}

// TODO the "model" nodes should have a first update step so that they
// can share data across all renderers during a tick
using video_decoder = ::Video::VideoInterface;

struct GPUVideoDecoder
{
  virtual ~GPUVideoDecoder() {}
  virtual void init(Renderer& r, RenderedNode& rendered)  = 0;
  virtual void exec(Renderer&, RenderedNode& rendered, QRhiResourceUpdateBatch& res, AVFrame& frame) = 0;
  virtual void release(Renderer&, RenderedNode& rendered) = 0;

  static inline
  QRhiTextureSubresourceUploadDescription createTextureUpload(uint8_t* pixels, int w, int h, int bytesPerPixel, int stride)
  {
    QRhiTextureSubresourceUploadDescription subdesc;

    const int rowBytes = w * bytesPerPixel;
    if(rowBytes == stride)
    {
      subdesc.setData(QByteArray::fromRawData(reinterpret_cast<const char*>(pixels), rowBytes * h));
    }
    else
    {
      QByteArray data{w * h, Qt::Uninitialized};
      for(int r = 0; r < h; r++)
      {
        const char* input = reinterpret_cast<const char*>(pixels + stride * r);
        char* output = data.data() + rowBytes * r;
        std::copy(input, input + rowBytes, output);
      }
      subdesc.setData(data);
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
  { }

  NodeModel& node;
  void init(Renderer& r, RenderedNode& rendered) override
  {
    std::tie(node.m_vertexS, node.m_fragmentS) = makeShaders(node.mesh().defaultVertexShader(), hashtag_no_filter);
  }

  void exec(Renderer&, RenderedNode& rendered, QRhiResourceUpdateBatch& res, AVFrame& frame) override
  {
  }

  void release(Renderer&, RenderedNode& n) override
  {
  }
};


