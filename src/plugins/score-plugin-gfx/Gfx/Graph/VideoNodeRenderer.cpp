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
    case AV_PIX_FMT_YUVJ420P:
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
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, *m_decoder, filter);
      break;
    case AV_PIX_FMT_BGR0:
    case AV_PIX_FMT_BGRA:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, *m_decoder, filter);
      break;
    case AV_PIX_FMT_ARGB:
      // Go from ARGB  xyzw
      //      to RGBA  yzwx
      m_gpu = std::make_unique<PackedDecoder>(
            QRhiTexture::RGBA8, 4, *m_decoder, "processed.bgra = tex.yzwx; " + filter);
      break;
    case AV_PIX_FMT_ABGR:
      // Go from ABGR  xyzw
      //      to BGRA  yzwx
      m_gpu = std::make_unique<PackedDecoder>(
            QRhiTexture::BGRA8, 4, *m_decoder, "processed.bgra = tex.yzwx; " + filter);
      break;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GRAYF32LE:
    case AV_PIX_FMT_GRAYF32BE:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::R32F, 4, *m_decoder, filter);
      break;
#endif
    case AV_PIX_FMT_GRAY8:
      m_gpu = std::make_unique<PackedDecoder>(
          QRhiTexture::R8, 1, *m_decoder,  "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + filter);
      break;
    case AV_PIX_FMT_GRAY16:
      m_gpu = std::make_unique<PackedDecoder>(
            QRhiTexture::R16, 2, *m_decoder, filter);
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

void VideoNodeRenderer::runRenderPass(RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
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
}

AVFrame* VideoNodeRenderer::nextFrame()
{
  auto& nodem = const_cast<VideoNode&>(static_cast<const VideoNode&>(node));
  auto& decoder = *m_decoder;

  //double expected_frame = nodem.standardUBO.time / decoder.fps;
  double current_flicks = nodem.standardUBO.time * ossia::flicks_per_second<double>;
  double flicks_per_frame = ossia::flicks_per_second<double> / 25.;

  ossia::small_vector<AVFrame*, 8> prev{};

  if(auto frame = m_nextFrame)
  {
    auto drift_in_frames = (current_flicks - decoder.flicks_per_dts * frame->pts) / flicks_per_frame;

    if (abs(drift_in_frames) <= 1.)
    {
      // we can finally show this frame
      m_nextFrame = nullptr;
      return frame;
    }
    else if(abs(drift_in_frames) > 5.)
    {
      // we likely seeked, move on to the dequeue
      prev.push_back(frame);
      m_nextFrame = nullptr;
    }
    else if(drift_in_frames < -1.)
    {
      // we early, keep showing the current frame (e.g. do nothing)
      return nullptr;
    }
    else if(drift_in_frames > 1.)
    {
      // we late, move on to the dequeue
      prev.push_back(frame);
      m_nextFrame = nullptr;
    }
  }

  while (auto frame = decoder.dequeue_frame())
  {
    auto drift_in_frames = (current_flicks - decoder.flicks_per_dts * frame->pts) / flicks_per_frame;

    if (abs(drift_in_frames) <= 1.)
    {
      m_framesToFree.insert(m_framesToFree.end(), prev.begin(), prev.end());
      return frame;
    }
    else if(abs(drift_in_frames) > 5.)
    {
      // we likely seeked, dequeue
      prev.push_back(frame);
      if(prev.size() >= 8)
        break;

      continue;
    }
    else if(drift_in_frames < -1.)
    {
      //current_time < frame_time: we are in advance by more than one frame, keep showing the current frame
      m_nextFrame = frame;
      m_framesToFree.insert(m_framesToFree.end(), prev.begin(), prev.end());
      return nullptr;
    }
    else if(drift_in_frames > 1.)
    {
      //current_time > frame_time: we are late by more than one frame, fetch new frames
      prev.push_back(frame);
      if(prev.size() >= 8)
        break;

      continue;
    }

    m_framesToFree.insert(m_framesToFree.end(), prev.begin(), prev.end());
    return frame;
  }

  switch(prev.size())
  {
    case 0:
    {
      return nullptr;
    }
    case 1:
    {
      return prev.back(); // display the closest frame we got
    }
    default:
    {
      for(std::size_t i = 0; i < prev.size() - 1; ++i) {
        m_framesToFree.push_back(prev[i]);
      }
      return prev.back();
    }
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

  auto& decoder = *m_decoder;

  // Release frames from the previous update (which have necessarily been uploaded)
  for (auto frame : m_framesToFree)
    decoder.release_frame(frame);
  m_framesToFree.clear();

  // Camera and other sources where we always want the latest frame
  if(decoder.realTime)
  {
    if (auto frame = decoder.dequeue_frame())
    {
      displayRealTimeFrame(*frame, renderer, res);
    }
  }
  else if (mustReadVideoFrame())
  {
    // Video files which require more precise timing handling
    if (auto frame = nextFrame())
    {
      displayVideoFrame(*frame, renderer, res);
    }
  }
}

void VideoNodeRenderer::displayRealTimeFrame(AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  checkFormat(
      renderer,
      static_cast<AVPixelFormat>(frame.format),
      frame.width,
      frame.height);

  if (m_gpu)
  {
    m_gpu->exec(renderer, res, frame);
  }

  m_framesToFree.push_back(&frame);
}

void VideoNodeRenderer::displayVideoFrame(AVFrame& frame, RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  auto& decoder = *m_decoder;

  checkFormat(
      renderer,
      static_cast<AVPixelFormat>(frame.format),
      frame.width,
      frame.height);
  if (m_gpu)
  {
    m_gpu->exec(renderer, res, frame);
  }

  m_lastFrameTime
      = (decoder.flicks_per_dts * frame.pts) / ossia::flicks_per_second<double>;

  m_framesToFree.push_back(&frame);
  m_timer.restart();
}

bool VideoNodeRenderer::mustReadVideoFrame()
{
  if(!std::exchange(m_readFrame, true))
    return true;

  auto& nodem = const_cast<VideoNode&>(static_cast<const VideoNode&>(node));
  auto& decoder = *m_decoder;

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
      = decoder.fps > 0
      ? (1. / (tempoRatio * decoder.fps))
      : (1. / 24.);
  next_frame_time += inv_fps;

  // If we are late
  if(current_time > next_frame_time)
    return true;

  // If we must display the next frame
  if(abs(m_timer.elapsed() - (1000. * inv_fps)) > 0.)
    return true;

  return false;
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
