#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/Graph/VideoNodeRenderer.hpp>
#include <Video/CameraInput.hpp>
#include <Video/FrameQueue.hpp>

#include <score/tools/Debug.hpp>
#include <score/tools/SafeCast.hpp>

#include <ossia/detail/flat_set.hpp>

#include <QGuiApplication>
namespace score::gfx
{
void VideoNodeBase::setScaleMode(ScaleMode s)
{
  m_scaleMode = s;
}

VideoNode::VideoNode(
    std::shared_ptr<Video::VideoInterface> dec, std::optional<double> nativeTempo)
    : m_nativeTempo{nativeTempo}
{
  this->reader.m_decoder = std::move(dec);
  output.push_back(new Port{this, {}, Types::Image, {}});
}

VideoNode::~VideoNode() { }

score::gfx::NodeRenderer* VideoNode::createRenderer(RenderList& r) const noexcept
{
  return new VideoNodeRenderer{
      *this, const_cast<VideoFrameShare&>(static_cast<const VideoFrameShare&>(reader))};
}

void VideoNode::seeked()
{
  SCORE_TODO;
}

void VideoNode::process(Message&& msg)
{
  m_lastToken = msg.token;

  m_timer.start();
}

void VideoNode::update()
{
  if(m_pause)
  {
    m_timer.restart();
    return;
  }

  ProcessNode::process(m_lastToken);
  auto elapsed = m_timer.nsecsElapsed() / 1e9;
  this->standardUBO.time += elapsed;

  reader.readNextFrame(*this);
}

void VideoNode::pause(bool b)
{
  m_pause = b;
}

CameraNode::CameraNode(std::shared_ptr<Video::ExternalInput> dec, QString f)
{
  this->m_filter = std::move(f);
  this->m_scaleMode = ScaleMode::Stretch;
  this->reader.m_decoder = std::move(dec);
  output.push_back(new Port{this, {}, Types::Image, {}});
}

CameraNode::~CameraNode() { }

score::gfx::NodeRenderer* CameraNode::createRenderer(RenderList& r) const noexcept
{
  return new VideoNodeRenderer{*this, const_cast<VideoFrameShare&>(reader)};
}

void CameraNode::process(Message&& msg)
{
  if(this->renderedNodes.size() > 0)
  {
    if(auto frame = reader.m_decoder->dequeue_frame())
    {
      reader.updateCurrentFrame(frame);
    }
  }

  reader.releaseFramesToFree();
}

void CameraNode::renderedNodesChanged()
{
  if(this->renderedNodes.size() == 0)
  {
    reader.releaseAllFrames();
    if(must_stop.exchange(false))
    {
      static_cast<::Video::ExternalInput*>(reader.m_decoder.get())->stop();
    }
  }
}

std::shared_ptr<RefcountedFrame> VideoFrameShare::currentFrame() const noexcept
{
  std::lock_guard<std::mutex> lock{m_frameLock};
  if(m_currentFrame)
  {
    m_currentFrame->use_count++;
  }
  return m_currentFrame;
}

void VideoFrameShare::updateCurrentFrame(AVFrame* frame)
{
  auto old_frame = this->m_currentFrame;

  {
    std::lock_guard l{m_frameLock};
    this->m_currentFrame = std::make_shared<RefcountedFrame>();
    this->m_currentFrame->frame = frame;
    this->m_currentFrame->use_count = 1;
  }

  if(old_frame)
  {
    if(old_frame->use_count == 1)
      m_framesToFree.push_back(old_frame->frame);
    else
      m_framesInFlight.push_back(std::move(old_frame));
  }

  m_currentFrameIdx++;
}

VideoFrameShare::VideoFrameShare() { }

VideoFrameShare::~VideoFrameShare()
{
  releaseAllFrames();
  QMetaObject::invokeMethod(
      qApp,
      [dec = std::move(m_decoder)] {

      },
      Qt::QueuedConnection);
}

void VideoFrameShare::releaseAllFrames()
{
  ossia::flat_set<AVFrame*> remaining;

  auto& decoder = *m_decoder;
  if(m_currentFrame && m_currentFrame->frame)
    remaining.insert(m_currentFrame->frame);
  m_currentFrame.reset();

  for(auto& frame : m_framesInFlight)
  {
    if(frame)
      remaining.insert(frame->frame);
  }
  m_framesInFlight.clear();

  for(auto frame : m_framesToFree)
  {
    remaining.insert(frame);
  }
  m_framesToFree.clear();

  for(auto f : remaining)
    decoder.release_frame(f);
}

void VideoFrameShare::releaseFramesToFree()
{
  // Give back the frames that aren't used by any thread anymore
  for(auto it = m_framesInFlight.begin(); it != m_framesInFlight.end();)
  {
    auto& frame = **it;
    if(frame.use_count == 1)
    {
      m_framesToFree.push_back(frame.frame);
      it = m_framesInFlight.erase(it);
    }
    else
    {
      ++it;
    }
  }

  auto& decoder = *m_decoder;

  // Release frames from the previous update (which have necessarily been uploaded)
  for(auto frame : m_framesToFree)
    decoder.release_frame(frame);
  m_framesToFree.clear();
}

VideoFrameReader::VideoFrameReader() { }

VideoFrameReader::~VideoFrameReader()
{
  m_framesToFree.push_back(m_nextFrame);
}

void VideoFrameReader::readNextFrame(VideoNode& node)
{
  auto& decoder = *m_decoder;

  if(mustReadVideoFrame(node))
  {
    // Video files which require more precise timing handling
    if(auto frame
       = VideoFrameReader::nextFrame(node, decoder, m_framesToFree, m_nextFrame))
    {
      updateCurrentFrame(frame);

      auto& nodem = const_cast<VideoNode&>(node);
      m_lastPlaybackTime = nodem.standardUBO.time;
      m_lastFrameTime
          = (decoder.flicks_per_dts * frame->pts) / ossia::flicks_per_second<double>;
      m_timer.restart();
    }
  }

  releaseFramesToFree();
}

void VideoFrameReader::pause(bool p)
{
  m_timer.restart();
}

bool VideoFrameReader::mustReadVideoFrame(const VideoNode& node)
{
  if(!std::exchange(m_readFrame, true))
    return true;

  auto& nodem = const_cast<VideoNode&>(node);
  auto& decoder = *m_decoder;

  double tempoRatio = 1.;
  if(nodem.m_nativeTempo)
  {
    if(*nodem.m_nativeTempo <= 0.)
    {
      return false;
    }

    tempoRatio = (*nodem.m_nativeTempo) / 120.;
  }

  auto current_time = nodem.standardUBO.time * tempoRatio; // In seconds
  auto next_frame_time = m_lastFrameTime;

  // what more can we do ?
  const double inv_fps
      = decoder.fps > 0 ? (1. / (tempoRatio * decoder.fps)) : (1. / 24.);
  next_frame_time += inv_fps;

  // If we are late
  if(current_time > next_frame_time)
    return true;

  // If we must display the next frame
  if(abs(m_timer.elapsed() - (1000. * inv_fps)) > 0.)
    return true;

  return false;
}

AVFrame* VideoFrameReader::nextFrame(
    const VideoNode& node, Video::VideoInterface& decoder,
    std::vector<AVFrame*>& m_framesToFree, AVFrame*& m_nextFrame)
{
  auto& nodem = const_cast<VideoNode&>(node);

  //double expected_frame = nodem.standardUBO.time / decoder.fps;
  double fps = decoder.fps > 0. ? decoder.fps : 24.;

  double current_flicks = nodem.standardUBO.time * ossia::flicks_per_second<double>;
  double flicks_per_frame = ossia::flicks_per_second<double> / fps;

  ossia::small_vector<AVFrame*, 8> prev{};

  if(auto frame = m_nextFrame)
  {
    auto drift_in_frames
        = (current_flicks - decoder.flicks_per_dts * frame->pts) / flicks_per_frame;

    if(abs(drift_in_frames) <= 1.)
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

  while(auto frame = decoder.dequeue_frame())
  {
    auto drift_in_frames
        = (current_flicks - decoder.flicks_per_dts * frame->pts) / flicks_per_frame;

    if(abs(drift_in_frames) <= 1.)
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
    case 0: {
      return nullptr;
    }
    case 1: {
      return prev.back(); // display the closest frame we got
    }
    default: {
      for(std::size_t i = 0; i < prev.size() - 1; ++i)
      {
        m_framesToFree.push_back(prev[i]);
      }
      return prev.back();
    }
  }
  return nullptr;
}

class BufferNodeRenderer : public NodeRenderer
{
public:
  const BufferNode& node;
  VideoFrameShare& reader;
  std::shared_ptr<RefcountedFrame> m_currentFrame{};
  int64_t m_currentFrameIdx{-1};
  explicit BufferNodeRenderer(
      const BufferNode& node, RenderList& r, VideoFrameShare& frames) noexcept;
  TextureRenderTarget renderTargetForInput(const Port& input) override { return {}; }

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override { }
  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override { }

  void update(RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto reader_frame = reader.m_currentFrameIdx;
    if(reader_frame > this->m_currentFrameIdx)
    {
      auto old_frame = m_currentFrame;

      m_currentFrame = reader.currentFrame();
      // if(m_currentFrame && m_currentFrame->frame)
      // {
      //   auto f = m_currentFrame->frame;
      //   qDebug() << " => " << f->data[0] << f->linesize[0] << f->format;
      // }

      if(old_frame)
        old_frame->use_count--;
      // TODO else ? fill with zeroes ?... does not that give green with YUV?

      this->m_currentFrameIdx = reader_frame;
    }
  }
  void release(RenderList& r) override
  {
    if(m_currentFrame)
    {
      m_currentFrame->use_count--;
      m_currentFrame.reset();
    }
  }
};

void BufferNode::renderedNodesChanged()
{
  if(this->renderedNodes.size() == 0)
  {
    reader.releaseAllFrames();
    if(must_stop.exchange(false))
    {
      safe_cast<::Video::ExternalInput*>(reader.m_decoder.get())->stop();
    }
  }
}

void BufferNode::process(Message&& msg)
{
  if(this->renderedNodes.size() > 0)
  {
    if(auto frame = reader.m_decoder->dequeue_frame())
    {
      reader.updateCurrentFrame(frame);
    }
  }

  reader.releaseFramesToFree();
}

BufferNode::BufferNode(std::shared_ptr<Video::ExternalInput> dec)
    : Node{}
{
  this->reader.m_decoder = std::move(dec);
  output.push_back(new Port{this, {}, Types::Buffer, {}});
}

NodeRenderer* BufferNode::createRenderer(RenderList& r) const noexcept
{
  auto& reader
      = const_cast<VideoFrameShare&>(static_cast<const VideoFrameShare&>(this->reader));
  return new BufferNodeRenderer{*this, r, reader};
};

BufferNodeRenderer::BufferNodeRenderer(
    const BufferNode& node, RenderList& r, VideoFrameShare& frames) noexcept
    : NodeRenderer{}
    , node{node}
    , reader{frames}
{
}
}

#include <hap/source/hap.c>
