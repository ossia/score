#pragma once
#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>
}

#include <fmt/format.h>

#include <string>

namespace Video
{
/// The libavdevice options that ask a camera for a mode. Caller frees.
inline AVDictionary* cameraOpenOptions(
    AVCodecID codec, AVPixelFormat pixfmt, double fps, int width, int height) noexcept
{
  AVDictionary* options = nullptr;

  if(codec != AV_CODEC_ID_NONE)
    if(auto codec_name = avcodec_get_name(codec))
      av_dict_set(&options, "input_format", codec_name, 0);

  if(pixfmt != AV_PIX_FMT_NONE)
    if(auto name = av_get_pix_fmt_name(pixfmt))
      av_dict_set(&options, "pixel_format", name, 0);

  if(fps > 0.)
    av_dict_set(&options, "framerate", std::to_string(fps).c_str(), 0);

  if(width > 0 && height > 0)
    av_dict_set(&options, "video_size", fmt::format("{}x{}", width, height).c_str(), 0);

  return options;
}
}
#endif
