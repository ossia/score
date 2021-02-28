#pragma once
#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

#include <score_plugin_media_export.h>
extern "C"
{
#include <libavformat/avformat.h>
}

namespace Video
{
struct SCORE_PLUGIN_MEDIA_EXPORT VideoMetadata
{
  int width{};
  int height{};
  double fps{};
  AVPixelFormat pixel_format{};
  bool realTime{};
  double flicks_per_dts{};
  double dts_per_flicks{};
};

struct SCORE_PLUGIN_MEDIA_EXPORT VideoInterface
    : VideoMetadata
{
  virtual ~VideoInterface();
  virtual AVFrame* dequeue_frame() noexcept = 0;
  virtual void release_frame(AVFrame* frame) noexcept = 0;
};

}
#endif
