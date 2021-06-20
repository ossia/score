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

VideoNodeRenderer::VideoNodeRenderer(const VideoNode& node) noexcept
    : NodeRenderer{}
    , node{node}
    , m_decoder{node.m_decoder} // TODO clone. But how to do for camera, etc. ?
    , m_currentFormat{m_decoder->pixel_format}
    , m_currentWidth{m_decoder->width}
    , m_currentHeight{m_decoder->height}
{
}

VideoNodeRenderer::~VideoNodeRenderer()
{
  auto& decoder = *m_decoder;
  for (auto frame : m_framesToFree)
    decoder.release_frame(frame);
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
      m_gpu = std::make_unique<YUV420Decoder>(*m_decoder);
      break;
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUV422P:
      m_gpu = std::make_unique<YUV422Decoder>(*m_decoder);
      break;
    case AV_PIX_FMT_UYVY422:
      m_gpu = std::make_unique<UYVY422Decoder>(*m_decoder);
      break;
    case AV_PIX_FMT_YUYV422:
      m_gpu = std::make_unique<YUYV422Decoder>(*m_decoder);
      break;
    case AV_PIX_FMT_RGB0:
    case AV_PIX_FMT_RGBA:
      m_gpu = std::make_unique<RGB0Decoder>(
          QRhiTexture::RGBA8, *m_decoder, filter);
      break;
    case AV_PIX_FMT_BGR0:
    case AV_PIX_FMT_BGRA:
      m_gpu = std::make_unique<RGB0Decoder>(
          QRhiTexture::BGRA8, *m_decoder, filter);
      break;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GRAYF32LE:
    case AV_PIX_FMT_GRAYF32BE:
      m_gpu = std::make_unique<RGB0Decoder>(
          QRhiTexture::R32F, *m_decoder, filter);
      break;
#endif
    case AV_PIX_FMT_GRAY8:
      m_gpu = std::make_unique<RGB0Decoder>(
          QRhiTexture::R8, *m_decoder, filter);
      break;
    default:
    {
      // try to read format as a 4cc
      std::string_view fourcc{(const char*)&m_currentFormat, 4};

      if (fourcc == "Hap1")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC1, *m_decoder, filter);
      else if (fourcc == "Hap5")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, *m_decoder, filter);
      else if (fourcc == "HapY")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3,
            *m_decoder,
            HAPDefaultDecoder::ycocg_filter + filter);
      else if (fourcc == "HapM")
        m_gpu = std::make_unique<HAPMDecoder>(*m_decoder, filter);
      else if (fourcc == "HapA")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC4, *m_decoder, filter);
      else if (fourcc == "Hap7")
        m_gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC7, *m_decoder, filter);

      if (!m_gpu)
      {
        qDebug() << "Unhandled pixel format: "
                 << av_get_pix_fmt_name(m_currentFormat);
        m_gpu = std::make_unique<EmptyDecoder>();
      }
      break;
    }
  }
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

  if (m_gpu)
  {
    auto shaders = m_gpu->init(r);
    SCORE_ASSERT(m_p.empty());
    for(Edge* edge : this->node.output[0]->edges)
    {
      auto rt = r.renderTargetForOutput(*edge);
      if(rt.renderTarget)
      {
        auto& mesh = TexturedTriangle::instance();
        m_p.emplace_back(edge, score::gfx::buildPipeline(
              r,
              mesh,
              shaders.first,
              shaders.second,
              rt,
              m_processUBO,
              nullptr,
              m_gpu->samplers));
      }
    }
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
}

void VideoNodeRenderer::init(RenderList& renderer)
{
  auto& rhi = *renderer.state.rhi;

  auto& mesh = TexturedTriangle::instance();
  if (!m_meshBuffer)
  {
    auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
    m_meshBuffer = mbuffer;
    m_idxBuffer = ibuffer;
  }

  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
#include <Gfx/Qt5CompatPush> // clang-format: keep
  m_processUBO->create();
#include <Gfx/Qt5CompatPop> // clang-format: keep

  if (!m_gpu)
  {
    createGpuDecoder();
  }

  if (m_gpu)
  {
    auto shaders = m_gpu->init(renderer);

    SCORE_ASSERT(m_p.empty());
    for(Edge* edge : this->node.output[0]->edges)
    {
      auto rt = renderer.renderTargetForOutput(*edge);
      if(rt.renderTarget)
      {
        m_p.emplace_back(edge, score::gfx::buildPipeline(renderer, mesh, shaders.first, shaders.second, rt, m_processUBO, nullptr, m_gpu->samplers));
      }
    }
  }
}

QRhiResourceUpdateBatch* VideoNodeRenderer::runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  auto it = ossia::find_if(m_p, [ptr=&edge] (const auto& p){ return p.first == ptr; });
  SCORE_ASSERT(it != m_p.end());
  {
    const auto sz = renderer.state.size;
    cb.setGraphicsPipeline(it->second.pipeline);
    cb.setShaderResources(it->second.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    assert(this->m_meshBuffer);
    assert(this->m_meshBuffer->usage().testFlag(QRhiBuffer::VertexBuffer));
    auto& mesh = TexturedTriangle::instance();

    mesh.setupBindings(*this->m_meshBuffer, this->m_idxBuffer, cb);

    cb.draw(mesh.vertexCount);
  }
  return nullptr;
}

// TODO if we have multiple renderers for the same video, we must always keep
// a frame because rendered may have different rates, so we cannot know "when"
// all renderers have rendered, thue the pattern in the following function
// is not enough
void VideoNodeRenderer::update(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  res.updateDynamicBuffer(
      m_processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);

  auto& nodem = const_cast<VideoNode&>(static_cast<const VideoNode&>(node));
  auto& decoder = *m_decoder;
  for (auto frame : m_framesToFree)
    decoder.release_frame(frame);
  m_framesToFree.clear();

  // TODO
  auto mustReadFrame = [this, &decoder, &nodem] {
    double tempoRatio = 1.;
    if (nodem.m_nativeTempo)
      tempoRatio = (*nodem.m_nativeTempo) / 120.;

    auto current_time = nodem.standardUBO.time * tempoRatio; // In seconds
    auto next_frame_time = m_lastFrameTime;

    // pause
    if (nodem.standardUBO.time == m_lastPlaybackTime)
    {
      return false;
    }
    m_lastPlaybackTime = nodem.standardUBO.time;

    // what more can we do ?
    const double inv_fps
        = decoder.fps > 0 ? 1. / (tempoRatio * decoder.fps) : 1. / 24.;
    next_frame_time += inv_fps;

    const bool we_are_late = current_time > next_frame_time;
    const bool timer = m_timer.elapsed() > (1000. * inv_fps);

    //const bool we_are_in_advance = std::abs(current_time - next_frame_time) > (2. * inv_fps);
    //const bool seeked = nodem.seeked.exchange(false);
    //const bool seeked_forward =
    return we_are_late || timer;
  };

  if (decoder.realTime || mustReadFrame())
  {
    if (auto frame = decoder.dequeue_frame())
    {
      checkFormat(
          renderer,
          static_cast<AVPixelFormat>(frame->format),
          frame->width,
          frame->height);
      if (m_gpu)
      {
        m_gpu->exec(renderer, res, *frame);
      }

      int64_t ts = frame->best_effort_timestamp;
      m_lastFrameTime
          = (decoder.flicks_per_dts * ts) / ossia::flicks_per_second<double>;

      //qDebug() << m_lastFrameTime << node.standardUBO.time;
      //qDebug() << (m_lastFrameTime - nodem.standardUBO.time);

      m_framesToFree.push_back(frame);
    }
    m_timer.restart();
  }
}

void VideoNodeRenderer::release(RenderList& r)
{
  if (m_gpu)
    m_gpu->release(r);

  delete m_processUBO;
  m_processUBO = nullptr;

  for(auto& p : m_p)
    p.second.release();
  m_p.clear();

  m_meshBuffer = nullptr;
}

}
