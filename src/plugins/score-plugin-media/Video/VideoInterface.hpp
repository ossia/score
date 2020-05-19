#pragma once
#include <score_plugin_media_export.h>
extern "C"
{
#include <libavformat/avformat.h>
}

namespace Video
{
struct SCORE_PLUGIN_MEDIA_EXPORT VideoInterface
{
  virtual ~VideoInterface();
  virtual AVFrame* dequeue_frame() noexcept = 0;
  virtual void release_frame(AVFrame* frame) noexcept = 0;

   int width{};
   int height{};
   double fps{};
   AVPixelFormat pixel_format{};
};

}
