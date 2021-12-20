#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <Video/VideoInterface.hpp>
#include <Video/FrameQueue.hpp>

extern "C"
{
struct SwsContext;
}

#include <QDebug>
#include <score_plugin_media_export.h>
namespace Video
{
class SCORE_PLUGIN_MEDIA_EXPORT Rescale
{
public:
  operator bool() const noexcept { return m_rescale; }

  void open(const VideoMetadata& src);
  void close();
  void rescale(const VideoMetadata& src, FrameQueue& m_frames, AVFramePointer& frame, ReadFrame& read);

private:
  SwsContext* m_rescale{};
};
}
#endif
