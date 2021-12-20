#pragma once
#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
}

namespace Video
{
inline
bool formatNeedsDecoding(AVPixelFormat fmt)
{
  switch (fmt)
  {
    // Supported formats for gpu decoding
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_UYVY422:
    case AV_PIX_FMT_YUYV422:
    case AV_PIX_FMT_RGB0:
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_BGR0:
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_ARGB:
    case AV_PIX_FMT_ABGR:

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GRAYF32LE:
    case AV_PIX_FMT_GRAYF32BE:
#endif
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_GRAY16:
      return false;

    // Other formats get rgb'd
    default:
      return true;
  }
}

}

#endif
