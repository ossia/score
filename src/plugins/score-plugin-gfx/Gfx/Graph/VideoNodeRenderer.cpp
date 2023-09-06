#include <Gfx/Graph/VideoNodeRenderer.hpp>
#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/HAP.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV420P10.hpp>
#include <Gfx/Graph/decoders/YUV420P12.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/YUV422P10.hpp>
#include <Gfx/Graph/decoders/YUV422P12.hpp>
#include <Gfx/Graph/decoders/YUYV422.hpp>
#include <Video/VideoDecoder.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/flicks.hpp>

#include <QElapsedTimer>

namespace score::gfx
{
std::unique_ptr<GPUVideoDecoder> createGpuDecoder(
    VideoNodeBase& model, const QString& filter, Video::ImageFormat& m_frameFormat)
{
  switch(m_frameFormat.pixel_format)
  {
    // 420P
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      return std::make_unique<YUV420Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV420P10LE:
      return std::make_unique<YUV420P10Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV420P12LE:
      return std::make_unique<YUV420P12Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_NV12:
      return std::make_unique<NV12Decoder>(m_frameFormat, false);
      break;
    case AV_PIX_FMT_NV21:
      return std::make_unique<NV12Decoder>(m_frameFormat, true);
      break;

      // 422P
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUV422P:
      return std::make_unique<YUV422Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV422P10LE:
      return std::make_unique<YUV422P10Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUV422P12LE:
      return std::make_unique<YUV422P12Decoder>(m_frameFormat);
      break;

      // YUYV
    case AV_PIX_FMT_UYVY422:
      return std::make_unique<UYVY422Decoder>(m_frameFormat);
      break;
    case AV_PIX_FMT_YUYV422:
      return std::make_unique<YUYV422Decoder>(m_frameFormat);
      break;

      // RGBA
    case AV_PIX_FMT_RGB0:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, m_frameFormat, "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_RGBA:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, m_frameFormat, filter);
      break;
    case AV_PIX_FMT_BGR0:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, m_frameFormat, "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_BGRA:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, m_frameFormat, filter);
      break;
    case AV_PIX_FMT_ARGB:
      // Go from ARGB  xyzw
      //      to RGBA  yzwx
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, m_frameFormat, "processed.rgba = tex.yzwx; " + filter);
      break;
    case AV_PIX_FMT_ABGR:
      // Go from ABGR  xyzw
      //      to BGRA  yzwx
      return std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, m_frameFormat, "processed.bgra = tex.yzwx; " + filter);
      break;

// Grey
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GBRPF32LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R32F, 4, "gbr", m_frameFormat, filter);
      break;
    case AV_PIX_FMT_GBRAPF32LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R32F, 4, "gbra", m_frameFormat, filter);
      break;
    case AV_PIX_FMT_GRAYF32LE:
    case AV_PIX_FMT_GRAYF32BE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R32F, 4, m_frameFormat, filter);
      break;
#endif

    case AV_PIX_FMT_GRAY8:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R8, 1, m_frameFormat,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;
    case AV_PIX_FMT_GRAY16:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R16, 2, m_frameFormat,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;
    default: {
      // try to read format as a 4cc
      std::string_view fourcc{(const char*)&m_frameFormat.pixel_format, 4};

      if(fourcc == "Hap1")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC1, m_frameFormat, filter);
      else if(fourcc == "Hap5")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, m_frameFormat, filter);
      else if(fourcc == "HapY")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, m_frameFormat, HAPDefaultDecoder::ycocg_filter + filter);
      else if(fourcc == "HapM")
        return std::make_unique<HAPMDecoder>(m_frameFormat, filter);
      else if(fourcc == "HapA")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC4, m_frameFormat, filter);
      else if(fourcc == "Hap7")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC7, m_frameFormat, filter);

      break;
    }
  }

  qDebug() << "Unhandled pixel format: "
           << av_get_pix_fmt_name(m_frameFormat.pixel_format);
  return std::make_unique<EmptyDecoder>();
}

TextureRenderTarget VideoNodeRendererBase::renderTargetForInput(const Port& input)
{
  return {};
}

void VideoNodeRendererBase::createGpuDecoder()
{
  auto& model = const_cast<VideoNodeBase&>(node);
  auto& filter = model.m_filter;
  qDebug() << "Creating ghpu with" << m_frameFormat.width << m_frameFormat.height;
  m_gpu = score::gfx::createGpuDecoder(model, filter, m_frameFormat);

  m_recomputeScale = true;
  m_currentFrameIdx = -1;
}

void VideoNodeRendererBase::setupGpuDecoder(RenderList& r)
{
  if(m_gpu)
  {
    m_gpu->release(r);

    for(auto& p : m_p)
    {
      p.second.release();
    }
    m_p.clear();
  }

  createGpuDecoder();

  createPipelines(r);
}

void VideoNodeRendererBase::createPipelines(RenderList& r)
{
  if(m_gpu)
  {
    auto shaders = m_gpu->init(r);
    SCORE_ASSERT(m_p.empty());
    score::gfx::defaultPassesInit(
        m_p, this->node.output[0]->edges, r, r.defaultTriangle(), shaders.first,
        shaders.second, m_processUBO, m_materialUBO, m_gpu->samplers);
  }
}

void VideoNodeRendererBase::checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h)
{
  // TODO won't work if VK is threaded and there are multiple windows
  if(!m_gpu || fmt != m_frameFormat.pixel_format || w != m_frameFormat.width
     || h != m_frameFormat.height)
  {
    m_frameFormat.pixel_format = fmt;
    m_frameFormat.width = w;
    m_frameFormat.height = h;

    setupGpuDecoder(r);
  }
}

void VideoNodeRendererBase::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  auto& rhi = *renderer.state.rhi;

  const auto& mesh = renderer.defaultQuad();
  if(!m_meshBuffer)
  {
    auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh, res);
    m_meshBuffer = mbuffer;
    m_idxBuffer = ibuffer;
  }

  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->create();

  m_materialUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Material));
  m_materialUBO->create();

  if(!m_gpu)
    createGpuDecoder();

  createPipelines(renderer);
  m_recomputeScale = true;
}

void VideoNodeRendererBase::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  score::gfx::quadRenderPass(
      renderer, {.mesh = m_meshBuffer, .index = m_idxBuffer}, cb, edge, m_p);
}

void VideoNodeRendererBase::release(RenderList& r)
{
  if(m_gpu)
    m_gpu->release(r);

  delete m_processUBO;
  m_processUBO = nullptr;

  delete m_materialUBO;
  m_materialUBO = nullptr;

  for(auto& p : m_p)
    p.second.release();
  m_p.clear();

  m_meshBuffer = nullptr;
}

VideoNodeRendererBase::VideoNodeRendererBase(const VideoNodeBase& node) noexcept
    : NodeRenderer{}
    , node{node}
{
}

VideoNodeRendererBase::~VideoNodeRendererBase() { }

void VideoNodeRendererBase::displayFrame(
    AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  if(frame.data[0] == nullptr)
    return;

  checkFormat(
      renderer, static_cast<AVPixelFormat>(frame.format), frame.width, frame.height);

  if(m_gpu)
  {
    m_gpu->exec(renderer, res, frame);
  }
}

void VideoNodeRendererBase::postprocess(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  if(m_recomputeScale || m_currentScaleMode != this->node.m_scaleMode)
  {
    m_currentScaleMode = this->node.m_scaleMode;
    auto sz = computeScale(
        m_currentScaleMode, renderer.state.renderSize,
        QSizeF(m_frameFormat.width, m_frameFormat.height));
    Material mat;
    mat.scale_w = sz.width();
    mat.scale_h = sz.height();

    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(Material), &mat);
    m_recomputeScale = false;
  }
}
//////////////////////

BasicVideoNodeRenderer::BasicVideoNodeRenderer(
    const VideoNodeBase& node,
    const std::shared_ptr<::Video::VideoDecoder>& frames) noexcept
    : VideoNodeRendererBase{node}
    , m_video{frames}
{
  m_frameFormat = *frames;
}

BasicVideoNodeRenderer::~BasicVideoNodeRenderer()
{
  if(m_curFrame)
    this->m_video->release_frame(m_curFrame);
}

// TODO if we have multiple renderers for the same video, we must always keep
// a frame because rendered may have different rates, so we cannot know "when"
// all renderers have rendered, thue the pattern in the following function
// is not enough
void BasicVideoNodeRenderer::update(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  res.updateDynamicBuffer(m_processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);

  m_frameFormat = *m_video;

  float t = this->node.standardUBO.time;

  int frame = int(t * this->m_video->fps);

  if(frame > m_prevFrame)
  {
    m_prevFrame = frame;

    if(auto frame = this->m_video->read_frame_impl())
    {
      displayFrame(*frame, renderer, res);

      if(m_curFrame)
      {
        this->m_video->release_frame(m_curFrame);
      }

      m_curFrame = frame;
    }
    else
    {
      this->m_video->seek(0);
      m_prevFrame = -1;
    }
  }

  postprocess(renderer, res);
}

///////////////////////

VideoNodeRenderer::VideoNodeRenderer(
    const VideoNodeBase& node, VideoFrameShare& frames) noexcept
    : VideoNodeRendererBase{node}
    , reader{frames}
{
  m_frameFormat = decoder();
}

VideoNodeRenderer::~VideoNodeRenderer() { }

Video::VideoMetadata& VideoNodeRenderer::decoder() const noexcept
{
  return *reader.m_decoder;
}

// TODO if we have multiple renderers for the same video, we must always keep
// a frame because rendered may have different rates, so we cannot know "when"
// all renderers have rendered, thue the pattern in the following function
// is not enough
void VideoNodeRenderer::update(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  res.updateDynamicBuffer(m_processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);

  auto reader_frame = reader.m_currentFrameIdx;
  if(reader_frame > this->m_currentFrameIdx)
  {
    auto old_frame = m_currentFrame;

    //std::lock_guard<std::mutex> lck{const_cast<VideoNode&>(node).reader.m_frameLock};
    if((m_currentFrame = reader.currentFrame()))
    {
      displayFrame(*m_currentFrame->frame, renderer, res);
    }

    if(old_frame)
      old_frame->use_count--;
    // TODO else ? fill with zeroes ?... does not that give green with YUV?

    this->m_currentFrameIdx = reader_frame;
  }

  postprocess(renderer, res);
}

}
