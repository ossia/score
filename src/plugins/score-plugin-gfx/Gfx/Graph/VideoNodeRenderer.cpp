#include <Gfx/Graph/VideoNodeRenderer.hpp>

#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/HAP.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/YUYV422.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/algorithms.hpp>

#include <QElapsedTimer>

namespace score::gfx
{

VideoNodeRenderer::VideoNodeRenderer(const VideoNodeBase& node, VideoFrameShare& frames) noexcept
    : NodeRenderer{}
    , node{node}
    , reader{frames}
    , m_currentFormat{decoder().pixel_format}
    , m_currentWidth{decoder().width}
    , m_currentHeight{decoder().height}
{
}

VideoNodeRenderer::~VideoNodeRenderer()
{
}


Video::VideoMetadata& VideoNodeRenderer::decoder() const noexcept
{
  return *reader.m_decoder;
}

TextureRenderTarget VideoNodeRenderer::renderTargetForInput(const Port& input)
{
  return {};
}

void VideoNodeRenderer::createGpuDecoder()
{
  auto& model = (VideoNode&)(node);
  auto& filter = model.m_filter;
  switch (m_currentFormat)
  {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      m_gpu = std::make_unique<YUV420Decoder>(this->decoder());
      break;
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUV422P:
      m_gpu = std::make_unique<YUV422Decoder>(this->decoder());
      break;
    case AV_PIX_FMT_UYVY422:
      m_gpu = std::make_unique<UYVY422Decoder>(this->decoder());
      break;
    case AV_PIX_FMT_YUYV422:
      m_gpu = std::make_unique<YUYV422Decoder>(this->decoder());
      break;
    case AV_PIX_FMT_RGB0:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, this->decoder(), "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_RGBA:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, this->decoder(), filter);
      break;
    case AV_PIX_FMT_BGR0:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, this->decoder(), "processed.a = 1.0; " + filter);
      break;
    case AV_PIX_FMT_BGRA:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, this->decoder(), filter);
      break;
    case AV_PIX_FMT_ARGB:
      // Go from ARGB  xyzw
      //      to RGBA  yzwx
      m_gpu = std::make_unique<PackedDecoder>(
            QRhiTexture::RGBA8, 4, this->decoder(), "processed.rgba = tex.yzwx; " + filter);
      break;
    case AV_PIX_FMT_ABGR:
      // Go from ABGR  xyzw
      //      to BGRA  yzwx
      m_gpu = std::make_unique<PackedDecoder>(
            QRhiTexture::BGRA8, 4, this->decoder(), "processed.bgra = tex.yzwx; " + filter);
      break;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GRAYF32LE:
    case AV_PIX_FMT_GRAYF32BE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::R32F, 4, this->decoder(), filter);
      break;
#endif
    case AV_PIX_FMT_GRAY8:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::R8, 1, this->decoder(), "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;
    case AV_PIX_FMT_GRAY16:
      m_gpu = std::make_unique<PackedDecoder>(
            QRhiTexture::R16, 2, this->decoder(), "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;
    default:
    {
      // try to read format as a 4cc
      std::string_view fourcc{(const char*)&m_currentFormat, 4};

      if (fourcc == "Hap1")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC1, this->decoder(), filter);
      else if (fourcc == "Hap5")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, this->decoder(), filter);
      else if (fourcc == "HapY")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3,
            this->decoder(),
            HAPDefaultDecoder::ycocg_filter + filter);
      else if (fourcc == "HapM")
        m_gpu = std::make_unique<HAPMDecoder>(this->decoder(), filter);
      else if (fourcc == "HapA")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC4, this->decoder(), filter);
      else if (fourcc == "Hap7")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC7, this->decoder(), filter);

      if (!m_gpu)
      {
        qDebug() << "Unhandled pixel format: "
                 << av_get_pix_fmt_name(m_currentFormat);
        m_gpu = std::make_unique<EmptyDecoder>();
      }
      break;
    }
  }

  m_recomputeScale = true;
  m_currentFrameIdx = -1;
}

void VideoNodeRenderer::setupGpuDecoder(RenderList& r)
{
  if (m_gpu)
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

void VideoNodeRenderer::createPipelines(RenderList& r)
{
  if (m_gpu)
  {
    auto shaders = m_gpu->init(r);
    SCORE_ASSERT(m_p.empty());
    score::gfx::defaultPassesInit(
        m_p,
        this->node.output[0]->edges,
        r,
        r.defaultTriangle(),
        shaders.first,
        shaders.second,
        m_processUBO,
        m_materialUBO,
        m_gpu->samplers);
  }
}

void VideoNodeRenderer::checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h)
{
  // TODO won't work if VK is threaded and there are multiple windows
  if (!m_gpu || fmt != m_currentFormat || w != m_currentWidth
      || h != m_currentHeight)
  {
    m_currentFormat = fmt;
    m_currentWidth = w;
    m_currentHeight = h;
    setupGpuDecoder(r);
  }
  else
  {
    m_currentFormat = fmt;
    m_currentWidth = w;
    m_currentHeight = h;
  }
}

void VideoNodeRenderer::init(RenderList& renderer)
{
  auto& rhi = *renderer.state.rhi;

  const auto& mesh = renderer.defaultQuad();
  if (!m_meshBuffer)
  {
    auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
    m_meshBuffer = mbuffer;
    m_idxBuffer = ibuffer;
  }

  #include <Gfx/Qt5CompatPush> // clang-format: keep
  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->create();

  m_materialUBO = rhi.newBuffer(
  QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(Material));
  m_materialUBO->create();
  #include <Gfx/Qt5CompatPop> // clang-format: keep

  if (!m_gpu)
    createGpuDecoder();

  createPipelines(renderer);
  m_recomputeScale = true;
}

void VideoNodeRenderer::runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  score::gfx::quadRenderPass(m_meshBuffer, m_idxBuffer, renderer, cb, edge, m_p);
}

// TODO if we have multiple renderers for the same video, we must always keep
// a frame because rendered may have different rates, so we cannot know "when"
// all renderers have rendered, thue the pattern in the following function
// is not enough
void VideoNodeRenderer::update(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  res.updateDynamicBuffer(
      m_processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);

  auto reader_frame = reader.m_currentFrameIdx;
  if(reader_frame > this->m_currentFrameIdx)
  {
    auto old_frame = m_currentFrame;

    //std::lock_guard<std::mutex> lck{const_cast<VideoNode&>(node).reader.m_frameLock};
    if((m_currentFrame = reader.currentFrame()))
      displayFrame(*m_currentFrame->frame, renderer, res);

    if(old_frame)
      old_frame->use_count--;
    // TODO else ? fill with zeroes ?... does not that give green with YUV?

    this->m_currentFrameIdx = reader_frame;
  }

  if(m_recomputeScale || m_currentScaleMode != this->node.m_scaleMode)
  {
    m_currentScaleMode = this->node.m_scaleMode;
    auto sz = computeScale(m_currentScaleMode, renderer.state.renderSize, QSizeF(m_currentWidth, m_currentHeight));
    Material mat;
    mat.scale_w = sz.width();
    mat.scale_h = sz.height();

    res.updateDynamicBuffer(m_materialUBO, 0, sizeof(Material), &mat);
    m_recomputeScale = false;
  }
}

void VideoNodeRenderer::displayFrame(AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  if(frame.data[0] == nullptr)
    return;

  checkFormat(
      renderer,
      static_cast<AVPixelFormat>(frame.format),
      frame.width,
      frame.height);

  if (m_gpu)
  {
    m_gpu->exec(renderer, res, frame);
  }
}


void VideoNodeRenderer::release(RenderList& r)
{
  if (m_gpu)
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

}
