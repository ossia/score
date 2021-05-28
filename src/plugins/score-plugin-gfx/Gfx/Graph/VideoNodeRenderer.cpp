#include <Gfx/Graph/VideoNodeRenderer.hpp>

#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/HAP.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/YUYV422.hpp>

#include <ossia/detail/flicks.hpp>

#include <QElapsedTimer>

namespace score::gfx
{

VideoNodeRenderer::VideoNodeRenderer(const VideoNode& node) noexcept
    : NodeRenderer{}
    , node{node}
    , decoder{node.m_decoder} // TODO clone. But how to do for camera, etc. ?
    , current_format{decoder->pixel_format}
    , current_width{decoder->width}
    , current_height{decoder->height}
{
}

VideoNodeRenderer::~VideoNodeRenderer()
{
  auto& decoder = *static_cast<const VideoNode&>(node).m_decoder;
  for (auto frame : framesToFree)
    decoder.release_frame(frame);
}

void VideoNodeRenderer::createGpuDecoder()
{
  auto& model = (VideoNode&)(node);
  auto& filter = model.m_filter;
  switch (current_format)
  {
    case AV_PIX_FMT_YUV420P:
      gpu = std::make_unique<YUV420Decoder>(*decoder);
      break;
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUV422P:
      gpu = std::make_unique<YUV422Decoder>(*decoder);
      break;
    case AV_PIX_FMT_UYVY422:
      gpu = std::make_unique<UYVY422Decoder>(*decoder);
      break;
    case AV_PIX_FMT_YUYV422:
      gpu = std::make_unique<YUYV422Decoder>(*decoder);
      break;
    case AV_PIX_FMT_RGB0:
    case AV_PIX_FMT_RGBA:
      gpu = std::make_unique<RGB0Decoder>(
          QRhiTexture::RGBA8, *decoder, filter);
      break;
    case AV_PIX_FMT_BGR0:
    case AV_PIX_FMT_BGRA:
      gpu = std::make_unique<RGB0Decoder>(
          QRhiTexture::BGRA8, *decoder, filter);
      break;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GRAYF32LE:
    case AV_PIX_FMT_GRAYF32BE:
      gpu = std::make_unique<RGB0Decoder>(
          QRhiTexture::R32F, *decoder, filter);
      break;
#endif
    case AV_PIX_FMT_GRAY8:
      gpu = std::make_unique<RGB0Decoder>(
          QRhiTexture::R8, *decoder, filter);
      break;
    default:
    {
      // try to read format as a 4cc
      std::string_view fourcc{(const char*)&current_format, 4};

      if (fourcc == "Hap1")
        gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC1, *decoder, filter);
      else if (fourcc == "Hap5")
        gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, *decoder, filter);
      else if (fourcc == "HapY")
        gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3,
            *decoder,
            HAPDefaultDecoder::ycocg_filter + filter);
      else if (fourcc == "HapM")
        gpu = std::make_unique<HAPMDecoder>(*decoder, filter);
      else if (fourcc == "HapA")
        gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC4, *decoder, filter);
      else if (fourcc == "Hap7")
        gpu = std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC7, *decoder, filter);

      if (!gpu)
      {
        qDebug() << "Unhandled pixel format: "
                 << av_get_pix_fmt_name(current_format);
        gpu = std::make_unique<EmptyDecoder>();
      }
      break;
    }
  }
}

void VideoNodeRenderer::setupGpuDecoder(RenderList& r)
{
  if (gpu)
  {
    gpu->release(r);
    delete m_p.pipeline;
    m_p.pipeline = nullptr;
  }
  createGpuDecoder();

  if (gpu)
  {
    auto shaders = gpu->init(r);
    m_p = score::gfx::buildPipeline(
        r,
        node.mesh(),
        std::move(shaders.first),
        std::move(shaders.second),
        m_rt,
        m_processUBO,
        nullptr,
        gpu->m_samplers);
  }
}

void VideoNodeRenderer::checkFormat(RenderList& r, AVPixelFormat fmt, int w, int h)
{
  // TODO won't work if VK is threaded and there are multiple windows
  if (!gpu || fmt != current_format || w != current_width
      || h != current_height)
  {
    current_format = fmt;
    current_width = w;
    current_height = h;
    setupGpuDecoder(r);
  }
}

TextureRenderTarget VideoNodeRenderer::createRenderTarget(const RenderState& state)
{
  auto sz = state.size;
  if (auto true_sz = renderTargetSize())
  {
    sz = *true_sz;
  }

  m_rt = score::gfx::createRenderTarget(state, QRhiTexture::RGBA8, sz);
  return m_rt;
}

void VideoNodeRenderer::init(RenderList& renderer)
{
  if (!m_rt.renderTarget)
    createRenderTarget(renderer.state);

  auto& rhi = *renderer.state.rhi;

  const auto& mesh = node.mesh();
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

  if (!gpu)
  {
    createGpuDecoder();
  }

  if (gpu)
  {
    auto shaders = gpu->init(renderer);
    if (!m_p.pipeline)
    {
      // Build the pipeline
      m_p = score::gfx::buildPipeline(
          renderer,
          node.mesh(),
          shaders.first,
          shaders.second,
          m_rt,
          m_processUBO,
          nullptr,
          gpu->m_samplers);
    }
  }
}

void VideoNodeRenderer::runPass(RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch& updateBatch)
{
  update(renderer, updateBatch);

  cb.beginPass(m_rt.renderTarget, Qt::black, {1.0f, 0}, &updateBatch);
  {
    const auto sz = renderer.state.size;
    cb.setGraphicsPipeline(m_p.pipeline);
    cb.setShaderResources(m_p.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    assert(this->m_meshBuffer);
    assert(this->m_meshBuffer->usage().testFlag(QRhiBuffer::VertexBuffer));
    node.mesh().setupBindings(*this->m_meshBuffer, this->m_idxBuffer, cb);

    cb.draw(node.mesh().vertexCount);
  }

  cb.endPass();
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
  auto& decoder = *nodem.m_decoder;
  for (auto frame : framesToFree)
    decoder.release_frame(frame);
  framesToFree.clear();

  // TODO
  auto mustReadFrame = [this, &decoder, &nodem] {
    double tempoRatio = 1.;
    if (nodem.m_nativeTempo)
      tempoRatio = (*nodem.m_nativeTempo) / 120.;

    auto current_time = nodem.standardUBO.time * tempoRatio; // In seconds
    auto next_frame_time = lastFrameTime;

    // pause
    if (nodem.standardUBO.time == lastPlaybackTime)
    {
      return false;
    }
    lastPlaybackTime = nodem.standardUBO.time;

    // what more can we do ?
    const double inv_fps
        = decoder.fps > 0 ? 1. / (tempoRatio * decoder.fps) : 1. / 24.;
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
      checkFormat(
          renderer,
          static_cast<AVPixelFormat>(frame->format),
          frame->width,
          frame->height);
      if (gpu)
      {
        gpu->exec(renderer, res, *frame);
      }

      int64_t ts = frame->best_effort_timestamp;
      lastFrameTime
          = (decoder.flicks_per_dts * ts) / ossia::flicks_per_second<double>;

      //qDebug() << lastFrameTime << node.standardUBO.time;
      //qDebug() << (lastFrameTime - nodem.standardUBO.time);

      framesToFree.push_back(frame);
    }
    t.restart();
  }
}

void VideoNodeRenderer::release(RenderList& r)
{
  releaseWithoutRenderTarget(r);
  m_rt.release();
}

void VideoNodeRenderer::releaseWithoutRenderTarget(RenderList& r)
{
  if (gpu)
    gpu->release(r);

  delete m_processUBO;
  m_processUBO = nullptr;

  m_p.release();

  m_meshBuffer = nullptr;
}

TextureRenderTarget VideoNodeRenderer::renderTarget() const noexcept
{
  return m_rt;
}

std::optional<QSize> VideoNodeRenderer::renderTargetSize() const noexcept
{
  return {};
}
}
