#pragma once

#include <Media/Libav.hpp>
#if SCORE_HAS_LIBAV

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
  LibavEncoder();
  ~LibavEncoder();
  void enumerate();
  void encode(AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt, FILE* outfile);

  int start();
  int add_frame(const unsigned char* data, AVPixelFormat fmt, int width, int height);
  int stop();

  bool available() const noexcept { return m_formatContext; }

  AVDictionary* opt = NULL;
  AVFormatContext* m_formatContext{};

  std::vector<OutputStream> streams;
};

}
#endif
