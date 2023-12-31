#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV
#include <Gfx/Libav/LibavOutputSettings.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}
namespace Gfx
{

struct OutputStream;
struct LibavEncoder
{
  explicit LibavEncoder(const LibavOutputSettings& set);
  ~LibavEncoder();
  void enumerate();

  int start();
  int add_frame(const unsigned char* data, AVPixelFormat fmt, int width, int height);
  int stop();

  bool available() const noexcept { return m_formatContext; }

  LibavOutputSettings m_set;
  AVDictionary* opt = NULL;
  AVFormatContext* m_formatContext{};

  std::vector<OutputStream> streams;
};

}
#endif
