#pragma once
#include <Media/Libav.hpp>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
struct AVCodecContext;
}

namespace Video
{
inline constexpr bool formatIsHardwareDecoded(AVPixelFormat fmt) noexcept
{
  switch(fmt)
  {
    case AV_PIX_FMT_VAAPI:
    case AV_PIX_FMT_VDPAU:
    case AV_PIX_FMT_DXVA2_VLD:
    case AV_PIX_FMT_D3D11:
    case AV_PIX_FMT_CUDA:
    case AV_PIX_FMT_QSV:
    case AV_PIX_FMT_VIDEOTOOLBOX:
      return true;
    default:
      return false;
  }
}

inline constexpr bool formatNeedsDecoding(AVPixelFormat fmt) noexcept
{
  // They all get translated to some NV12 or something like that
  if(formatIsHardwareDecoded(fmt))
    return false;

  switch(fmt)
  {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
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

// Get hardware pix format
struct HWAccelFormats
{
  AVPixelFormat format{AV_PIX_FMT_NONE};
  AVHWDeviceType device{AV_HWDEVICE_TYPE_NONE};
};

inline constexpr HWAccelFormats ffmpegHardwareDecodingFormats(AVPixelFormat p) noexcept
{
  switch(p)
  {
#if defined(__linux__)
    case AV_PIX_FMT_VAAPI:
      return {AV_PIX_FMT_VAAPI, AV_HWDEVICE_TYPE_VAAPI};
    case AV_PIX_FMT_VDPAU:
      return {AV_PIX_FMT_VDPAU, AV_HWDEVICE_TYPE_VDPAU};
#endif
#if defined(_WIN32)
    case AV_PIX_FMT_DXVA2_VLD:
      return {AV_PIX_FMT_DXVA2_VLD, AV_HWDEVICE_TYPE_DXVA2};
    case AV_PIX_FMT_D3D11:
      return {AV_PIX_FMT_D3D11, AV_HWDEVICE_TYPE_D3D11VA};
#endif
#if defined(__APPLE__)
    case AV_PIX_FMT_VIDEOTOOLBOX:
      return {AV_PIX_FMT_VIDEOTOOLBOX, AV_HWDEVICE_TYPE_VIDEOTOOLBOX};
#endif
      // Cross-platform pix formats
    case AV_PIX_FMT_CUDA:
      return {AV_PIX_FMT_CUDA, AV_HWDEVICE_TYPE_CUDA};
    case AV_PIX_FMT_QSV:
      return {AV_PIX_FMT_QSV, AV_HWDEVICE_TYPE_QSV};
      //       case AV_PIX_FMT_VULKAN:
      //         return {AV_PIX_FMT_VULKAN, AV_HWDEVICE_TYPE_VULKAN};
      //       case AV_PIX_FMT_OPENCL:
      //         return {AV_PIX_FMT_OPENCL, AV_HWDEVICE_TYPE_OPENCL};
    default:
      return {};
  }
}

inline constexpr bool ffmpegCanDoHardwareDecoding(AVCodecID id) noexcept
{
  switch(id)
  {
    case AV_CODEC_ID_AV1:
    case AV_CODEC_ID_H264:
    case AV_CODEC_ID_HEVC:
    case AV_CODEC_ID_MJPEG:
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
    case AV_CODEC_ID_MPEG4:
    case AV_CODEC_ID_VC1:
    case AV_CODEC_ID_VP8:
    case AV_CODEC_ID_VP9:
    case AV_CODEC_ID_WMV1:
    case AV_CODEC_ID_WMV2:
    case AV_CODEC_ID_WMV3:
      return true;
    default:
      return false;
  }
}

}

#endif
