#include <Gfx/Graph/videonode.hpp>

#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/YUYV422.hpp>

struct VideoNode::Rendered : RenderedNode
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

VideoNode::VideoNode(std::shared_ptr<video_decoder> dec, std::optional<double> nativeTempo, QString f)
    : decoder{std::move(dec)}
    , current_format{decoder->pixel_format}
    , nativeTempo{nativeTempo}
    , filter{f}
{
    initGpuDecoder();

    output.push_back(new Port{this, {}, Types::Image, {}});
}

void VideoNode::initGpuDecoder()
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
    // case AV_PIX_FMT_YUYV422:
    //     gpu = std::make_unique<YUYV422Decoder>(*this, *decoder);
    //     break;
    case AV_PIX_FMT_RGB0:
    case AV_PIX_FMT_RGBA:
        gpu = std::make_unique<RGB0Decoder>(QRhiTexture::RGBA8, *this, *decoder, filter);
        break;
    case AV_PIX_FMT_BGR0:
    case AV_PIX_FMT_BGRA:
        gpu = std::make_unique<RGB0Decoder>(QRhiTexture::BGRA8, *this, *decoder, filter);
        break;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GRAYF32LE:
    case AV_PIX_FMT_GRAYF32BE:
        gpu = std::make_unique<RGB0Decoder>(QRhiTexture::R32F, *this, *decoder, filter);
        break;
#endif
    case AV_PIX_FMT_GRAY8:
        gpu = std::make_unique<RGB0Decoder>(QRhiTexture::R8, *this, *decoder, filter);
        break;
    default:
        qDebug() << "Unhandled pixel format: " << av_get_pix_fmt_name(current_format);
        gpu = std::make_unique<EmptyDecoder>(*this);
        break;
    }
}

void VideoNode::checkFormat(AVPixelFormat fmt)
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

const Mesh& VideoNode::mesh() const noexcept { return this->m_mesh; }

VideoNode::~VideoNode() { }

score::gfx::NodeRenderer* VideoNode::createRenderer() const noexcept {
    auto r = new Rendered{*this};
    r->current_format = current_format;
    return r;
}
