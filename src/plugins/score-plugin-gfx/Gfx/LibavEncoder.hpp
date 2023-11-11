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

struct LibavEncoder
{
  LibavEncoder();
  void enumerate();
  void encode(AVCodecContext* enc_ctx, AVFrame* frame, AVPacket* pkt, FILE* outfile);

  AVDictionary* opt = NULL;

  int test2();
  void test();
  AVFormatContext* m_formatContext{};

#if 0
  AVFrame* frame;
  AVPacket* pkt;

  AVStream* m_avstream{};
  const AVCodec* m_codec{};
  AVCodecContext* m_codecContext{};
#endif
};

}
#endif
