#include <Gfx/Graph/SyncVideoNode.hpp>
#include <Gfx/Graph/VideoNodeRenderer.hpp>
#include <Video/SyncVideoDecoder.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/flicks.hpp>

namespace score::gfx
{

SyncVideoFrameReader::SyncVideoFrameReader() { }

SyncVideoFrameReader::~SyncVideoFrameReader()
{
  // Release any frames we're holding
  releaseAllFrames();
}

void SyncVideoFrameReader::readNextFrame(double currentTime)
{
  auto& decoder = *m_decoder;

  // Convert current time (seconds) to flicks
  int64_t target_flicks = currentTime * ossia::flicks_per_second<double>;

  // Avoid redundant decode requests for the same time
  if(target_flicks == m_lastRequestedFlicks && m_currentFrame)
  {
    return;
  }

  m_lastRequestedFlicks = target_flicks;

  // Request frame at exact time from sync decoder
  if(auto frame = decoder.decode_frame_at(target_flicks))
  {
    updateCurrentFrame(frame);
  }

  releaseFramesToFree();
}

SyncVideoNode::SyncVideoNode(
    std::shared_ptr<Video::VideoInterface> dec, std::optional<double> nativeTempo)
    : m_nativeTempo{nativeTempo}
{
  this->reader.m_decoder = std::move(dec);
  output.push_back(new Port{this, {}, Types::Image, {}});
}

SyncVideoNode::~SyncVideoNode() { }

score::gfx::NodeRenderer* SyncVideoNode::createRenderer(RenderList& r) const noexcept
{
  // Reuse VideoNodeRenderer - it just reads from VideoFrameShare
  return new VideoNodeRenderer{
      *this, const_cast<VideoFrameShare&>(static_cast<const VideoFrameShare&>(reader))};
}

void SyncVideoNode::process(Message&& msg)
{
  m_lastToken = msg.token;
  m_timer.start();
}

void SyncVideoNode::update()
{
  if(m_pause)
  {
    m_timer.restart();
    return;
  }

  ProcessNode::process(m_lastToken);
  auto elapsed = m_timer.nsecsElapsed() / 1e9;
  this->standardUBO.time += elapsed;

  // Calculate current playback time with tempo adjustment
  double tempoRatio = 1.;
  if(m_nativeTempo)
  {
    if(*m_nativeTempo <= 0.)
    {
      return;
    }
    tempoRatio = (*m_nativeTempo) / 120.;
  }

  double currentTime = this->standardUBO.time * tempoRatio;

  // Direct sync decode at the current time
  reader.readNextFrame(currentTime);
}

void SyncVideoNode::pause(bool b)
{
  m_pause = b;
}

}
