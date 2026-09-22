#pragma once

// <cstdint> first: libavutil/common.h refuses to be included from C++ unless
// UINT64_C is already defined.
#include <cstdint>
#include <string>

extern "C" {
#include <libavcodec/codec_desc.h>
#include <libavcodec/codec_id.h>
#include <libavutil/pixdesc.h>
#include <libavutil/pixfmt.h>
}

namespace Gfx
{
/// The name a camera format is offered under in the device list.
///
/// A compressed stream carries no pixel format, a raw one no codec: the codec
/// names the former, the layout the latter. Both lookups can answer nullptr,
/// so neither is assigned to the string unchecked.
inline std::string cameraFormatName(int codec, int pixelformat) noexcept
{
  if(codec != AV_CODEC_ID_NONE && codec != AV_CODEC_ID_RAWVIDEO)
  {
    if(const AVCodecDescriptor* desc = avcodec_descriptor_get((AVCodecID)codec))
      return desc->name;
  }

  if(const char* name = av_get_pix_fmt_name((AVPixelFormat)pixelformat))
    return name;

  return "unknown";
}
}
