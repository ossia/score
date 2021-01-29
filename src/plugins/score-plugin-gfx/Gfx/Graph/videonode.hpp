#pragma once

#include "node.hpp"
#include "renderer.hpp"
#include "renderstate.hpp"
#include "uniforms.hpp"
#include <QElapsedTimer>
#include <Video/VideoInterface.hpp>
#include <ossia/detail/flicks.hpp>
extern "C"
{
#include <libavutil/pixdesc.h>
}
// TODO the "model" nodes should have a first update step so that they
// can share data across all renderers during a tick
using video_decoder = ::Video::VideoInterface;

inline
QRhiTextureSubresourceUploadDescription createTextureUpload(uint8_t* pixels, int w, int h, int bytesPerPixel, int stride)
{
  QRhiTextureSubresourceUploadDescription subdesc;

  const int rowBytes = w * bytesPerPixel;
  qDebug() << w << h << stride ;
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

struct GPUVideoDecoder
{
  virtual ~GPUVideoDecoder() {}
  virtual void init(Renderer& r, RenderedNode& rendered)  = 0;
  virtual void exec(Renderer&, RenderedNode& rendered, QRhiResourceUpdateBatch& res, AVFrame& frame) = 0;
  virtual void release(Renderer&, RenderedNode& rendered) = 0;
};


struct YUV422Decoder : GPUVideoDecoder
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

// See softpixel.com/~cwright/programming/colorspace/yuv
const vec3 offset = vec3(0., -0.5, -0.5);
const mat3 coeff = mat3(1.0   ,  1.0   , 1.0,
                        0.0   , -0.3455, 1.7790,
                        1.4075, -0.7169, 0.);
void main()
{
    float y = texture(y_tex, v_texcoord).r;
    float u = texture(u_tex, v_texcoord).r;
    float v = texture(v_tex, v_texcoord).r;

    fragColor = vec4(coeff * (vec3(y,u,v) + offset), 1);
}
)_";

  YUV422Decoder(NodeModel& n, video_decoder& d): node{n}, decoder{d} { }
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
    const auto w = decoder.width / 2, h = decoder.height / 2;
    auto u_tex = rendered.m_samplers[1].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 1, stride)};

    QRhiTextureUploadDescription desc{entry};

    res.uploadTexture(u_tex, desc);
  }

  void setVPixels(RenderedNode& rendered, QRhiResourceUpdateBatch& res, uint8_t* pixels, int stride) const noexcept
  {
    const auto w = decoder.width / 2, h = decoder.height / 2;
    auto v_tex = rendered.m_samplers[2].texture;

    QRhiTextureUploadEntry entry{0, 0, createTextureUpload(pixels, w, h, 1, stride)};

    QRhiTextureUploadDescription desc{entry};
    res.uploadTexture(v_tex, desc);
  }
};

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

struct VideoNode : NodeModel
{
  std::shared_ptr<video_decoder> decoder;
  std::unique_ptr<GPUVideoDecoder> gpu;
  AVPixelFormat current_format = AV_PIX_FMT_YUV420P;
  std::atomic_bool seeked{};
  std::optional<double> nativeTempo;
  QString filter;

  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  VideoNode(std::shared_ptr<video_decoder> dec
            , std::optional<double> nativeTempo
            , QString f = {})
    : decoder{std::move(dec)}
    , current_format{decoder->pixel_format}
    , nativeTempo{nativeTempo}
    , filter{f}
  {
    initGpuDecoder();

    output.push_back(new Port{this, {}, Types::Image, {}});
  }

  void initGpuDecoder()
  {
    switch (current_format)
    {
      case AV_PIX_FMT_YUV420P:
        gpu = std::make_unique<YUV420Decoder>(*this, *decoder);
        break;
      case AV_PIX_FMT_YUVJ422P:
      case AV_PIX_FMT_YUV422P:
        gpu = std::make_unique<YUV422Decoder>(*this, *decoder);
        break;
      case AV_PIX_FMT_RGB0:
      case AV_PIX_FMT_RGBA:
        gpu = std::make_unique<RGB0Decoder>(QRhiTexture::RGBA8, *this, *decoder, filter);
        break;
      case AV_PIX_FMT_BGR0:
      case AV_PIX_FMT_BGRA:
        gpu = std::make_unique<RGB0Decoder>(QRhiTexture::BGRA8, *this, *decoder, filter);
        break;
      case AV_PIX_FMT_GRAYF32LE:
      case AV_PIX_FMT_GRAYF32BE:
        gpu = std::make_unique<RGB0Decoder>(QRhiTexture::R32F, *this, *decoder, filter);
        break;
      case AV_PIX_FMT_GRAY8:
        gpu = std::make_unique<RGB0Decoder>(QRhiTexture::R8, *this, *decoder, filter);
        break;
      default:
        qDebug() << "Unhandled pixel format: " << av_get_pix_fmt_name(current_format);
        gpu = std::make_unique<EmptyDecoder>(*this);
        break;
    }
  }
  void checkFormat(AVPixelFormat fmt)
  {
    // TODO won't work if VK is threaded and there are multiple windows
    if(fmt != current_format)
    {
      if(gpu)
      {
        for(auto& r : this->renderedNodes)
          r.second->releaseWithoutRenderTarget(*r.first);
      }
      current_format = fmt;
      initGpuDecoder();

      if(gpu)
      {
        for(auto& r : this->renderedNodes)
          r.second->init(*r.first);
      }
    }
  }

  const Mesh& mesh() const noexcept override { return this->m_mesh; }

  struct Rendered : RenderedNode
  {
    using RenderedNode::RenderedNode;
    std::vector<AVFrame*> framesToFree;
    AVPixelFormat current_format = AV_PIX_FMT_YUV420P;
    QElapsedTimer t;

    Rendered(const NodeModel& node) noexcept
      : RenderedNode{node}
    {

    }

    ~Rendered()
    {
      auto& decoder = *static_cast<const VideoNode&>(node).decoder;
      for (auto frame : framesToFree)
        decoder.release_frame(frame);
    }

    void customInit(Renderer& renderer) override
    {
      defaultShaderMaterialInit(renderer);

      auto& nodem = static_cast<const VideoNode&>(node);
      if(nodem.gpu)
        nodem.gpu->init(renderer, *this);
    }

    // TODO if we have multiple renderers for the same video, we must always keep
    // a frame because rendered may have different rates, so we cannot know "when"
    // all renderers have rendered, thue the pattern in the following function
    // is not enough
    double lastFrameTime{};
    void customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) override
    {
      auto& nodem = const_cast<VideoNode&>(static_cast<const VideoNode&>(node));
      auto& decoder = *nodem.decoder;
      for (auto frame : framesToFree)
        decoder.release_frame(frame);
      framesToFree.clear();


      // TODO
      auto mustReadFrame = [this, &decoder, &nodem] {
        double tempoRatio = 1.;
        if(nodem.nativeTempo)
          tempoRatio = (*nodem.nativeTempo) / 120.;

        auto current_time = nodem.standardUBO.time * tempoRatio; // In seconds
        auto next_frame_time = lastFrameTime;

        // what more can we do ?
        const double inv_fps = decoder.fps > 0
            ? 1. / (tempoRatio * decoder.fps)
            : 1. / 24.
        ;
        next_frame_time += inv_fps;

        const bool we_are_late = current_time > next_frame_time;
        const bool timer = t.elapsed() > (1000. * inv_fps);
        //const bool we_are_in_advance = std::abs(current_time - next_frame_time) > (2. * inv_fps);
        //const bool seeked = nodem.seeked.exchange(false);
        //const bool seeked_forward =
        return we_are_late || timer;
      };

      if (decoder.realTime || mustReadFrame())
      {
        if (auto frame = decoder.dequeue_frame())
        {
          nodem.checkFormat(static_cast<AVPixelFormat>(frame->format));
          if(nodem.gpu)
          {
            nodem.gpu->exec(renderer, *this, res, *frame);
          }

          int64_t ts = frame->best_effort_timestamp;
          lastFrameTime = (decoder.flicks_per_dts * ts) / ossia::flicks_per_second<double>;

          //qDebug() << lastFrameTime << node.standardUBO.time;
          //qDebug() << (lastFrameTime - nodem.standardUBO.time);

          framesToFree.push_back(frame);
        }
        t.restart();
      }
    }

    void customRelease(Renderer& r) override
    {
      auto& nodem = static_cast<const VideoNode&>(node);
      if(nodem.gpu)
        nodem.gpu->release(r, *this);
    }

  };

  virtual ~VideoNode() { }

  score::gfx::NodeRenderer* createRenderer() const noexcept override {
    auto r = new Rendered{*this};
    r->current_format = current_format;
    return r;
  }
};
