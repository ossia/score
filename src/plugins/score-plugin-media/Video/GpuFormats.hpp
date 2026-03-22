#pragma once
#include <Media/Libav.hpp>

#include <string>
#include <vector>

#if SCORE_HAS_LIBAV
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/version.h>
#if __has_include(<libavutil/hwcontext.h>)
#include <libavutil/hwcontext.h>
#endif
struct AVCodecContext;
}

namespace Video
{

inline bool hardwareDecoderIsAvailable(AVPixelFormat p) noexcept
{
  switch(p)
  {
#if defined(__linux__)
    case AV_PIX_FMT_DRM_PRIME: {
      static const bool ok = avcodec_find_decoder_by_name("h264_v4l2m2m");
      return ok;
    }
    case AV_PIX_FMT_VAAPI: {
      // FFmpeg 7+ removed dedicated VAAPI decoders (mjpeg_vaapi, etc.).
      // VAAPI now uses the generic decoder with hw_device_ctx.
      // Check if the VAAPI device type is supported instead.
      static const bool ok = avcodec_find_decoder_by_name("mjpeg_vaapi")
                             || av_hwdevice_find_type_by_name("vaapi") != AV_HWDEVICE_TYPE_NONE;
      return ok;
    }
    case AV_PIX_FMT_VDPAU: {
      static const bool ok = avcodec_find_decoder_by_name("h264_vdpau");
      return ok;
    }
#endif
#if defined(_WIN32)
    case AV_PIX_FMT_DXVA2_VLD:
      return true;
    case AV_PIX_FMT_D3D11:
      return true;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
    case AV_PIX_FMT_D3D12:
      return true;
#endif
#endif
#if defined(__APPLE__)
    case AV_PIX_FMT_VIDEOTOOLBOX:
      return true;
#endif
      // Cross-platform pix formats
    case AV_PIX_FMT_CUDA: {
      static const bool ok = avcodec_find_decoder_by_name("mjpeg_cuvid")
                             || avcodec_find_decoder_by_name("h264_cuvid");
      return ok;
    }
    case AV_PIX_FMT_QSV: {
      static const bool ok = avcodec_find_decoder_by_name("mjpeg_qsv")
                             || avcodec_find_decoder_by_name("h264_qsv");
      return ok;
    }
    case AV_PIX_FMT_VULKAN: {
      static const bool ok
          = av_hwdevice_find_type_by_name("vulkan") != AV_HWDEVICE_TYPE_NONE;
      return ok;
    }
    default:
      return false;
  }
}

inline constexpr bool formatIsHardwareDecoded(AVPixelFormat fmt) noexcept
{
#if LIBAVUTIL_VERSION_MAJOR < 57
  return false;
#else
  switch(fmt)
  {
    case AV_PIX_FMT_VAAPI:
    case AV_PIX_FMT_VDPAU:
    case AV_PIX_FMT_DXVA2_VLD:
    case AV_PIX_FMT_D3D11:
#if defined(_WIN32) && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
    case AV_PIX_FMT_D3D12:
#endif
    case AV_PIX_FMT_CUDA:
    case AV_PIX_FMT_QSV:
    case AV_PIX_FMT_VIDEOTOOLBOX:
    case AV_PIX_FMT_DRM_PRIME:
    case AV_PIX_FMT_VULKAN:
      return true;
    default:
      return false;
  }
#endif
}

inline constexpr bool formatNeedsDecoding(AVPixelFormat fmt) noexcept
{
  // They all get translated to some NV12 or something like that
  if(formatIsHardwareDecoded(fmt))
    return false;

  switch(fmt)
  {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_RGB24:
    case AV_PIX_FMT_BGR24:
    case AV_PIX_FMT_RGB48LE:
    case AV_PIX_FMT_BGR48LE:
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
    case AV_PIX_FMT_NV16:
    case AV_PIX_FMT_P010LE:
    case AV_PIX_FMT_P016LE:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUVJ440P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV440P:
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
    case AV_PIX_FMT_YUVA420P:
    case AV_PIX_FMT_YUVA444P:
    case AV_PIX_FMT_UYVY422:
    case AV_PIX_FMT_YUYV422:
    case AV_PIX_FMT_RGB0:
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_BGR0:
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_ARGB:
    case AV_PIX_FMT_ABGR:
    case AV_PIX_FMT_RGBA64LE:
    case AV_PIX_FMT_BGRA64LE:
    case AV_PIX_FMT_X2RGB10LE:
    case AV_PIX_FMT_GBRP:
    case AV_PIX_FMT_GBRAP:
    case AV_PIX_FMT_YA8:
    case AV_PIX_FMT_YA16LE:

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_YUV420P12LE:
    case AV_PIX_FMT_YUV422P10LE:
    case AV_PIX_FMT_YUV422P12LE:
    case AV_PIX_FMT_YUV444P10LE:
    case AV_PIX_FMT_YUV444P12LE:
    case AV_PIX_FMT_YUVA444P10LE:
    case AV_PIX_FMT_YUVA444P12LE:
    case AV_PIX_FMT_GBRP10LE:
    case AV_PIX_FMT_GBRP12LE:
    case AV_PIX_FMT_GBRP16LE:
    case AV_PIX_FMT_GBRAP10LE:
    case AV_PIX_FMT_GBRAP12LE:
    case AV_PIX_FMT_GBRAP16LE:
    case AV_PIX_FMT_GBRPF32LE:
    case AV_PIX_FMT_GBRAPF32LE:
    case AV_PIX_FMT_GRAYF32LE:
    case AV_PIX_FMT_GRAYF32BE:
    case AV_PIX_FMT_NV24:
    case AV_PIX_FMT_NV42:
    case AV_PIX_FMT_Y210LE:
#endif

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 17, 100)
    case AV_PIX_FMT_X2BGR10LE:
    case AV_PIX_FMT_P210LE:
    case AV_PIX_FMT_P410LE:
#endif

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(60, 8, 100)
    case AV_PIX_FMT_RGBAF32LE:
    case AV_PIX_FMT_VUYA:
    case AV_PIX_FMT_VUYX:
    case AV_PIX_FMT_GRAYF16LE:
    case AV_PIX_FMT_GRAYF16BE:
#endif

    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_GRAY16:
      return false;

    // Other formats get rgb'd
    default:
      return true;
  }
}

#if LIBAVUTIL_VERSION_MAJOR >= 57
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
    case AV_PIX_FMT_DRM_PRIME:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_DRM_PRIME))
        return {};
      return {AV_PIX_FMT_DRM_PRIME, AV_HWDEVICE_TYPE_DRM};
    case AV_PIX_FMT_VAAPI:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_VAAPI))
        return {};
      return {AV_PIX_FMT_VAAPI, AV_HWDEVICE_TYPE_VAAPI};
    case AV_PIX_FMT_VDPAU:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_VDPAU))
        return {};
      return {AV_PIX_FMT_VDPAU, AV_HWDEVICE_TYPE_VDPAU};
#endif
#if defined(_WIN32)
    case AV_PIX_FMT_DXVA2_VLD:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_DXVA2_VLD))
        return {};
      return {AV_PIX_FMT_DXVA2_VLD, AV_HWDEVICE_TYPE_DXVA2};
    case AV_PIX_FMT_D3D11:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_D3D11))
        return {};
      return {AV_PIX_FMT_D3D11, AV_HWDEVICE_TYPE_D3D11VA};
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
    case AV_PIX_FMT_D3D12:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_D3D12))
        return {};
      return {AV_PIX_FMT_D3D12, AV_HWDEVICE_TYPE_D3D12VA};
#endif
#endif
#if defined(__APPLE__)
    case AV_PIX_FMT_VIDEOTOOLBOX:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_VIDEOTOOLBOX))
        return {};
      return {AV_PIX_FMT_VIDEOTOOLBOX, AV_HWDEVICE_TYPE_VIDEOTOOLBOX};
#endif
      // Cross-platform pix formats
    case AV_PIX_FMT_CUDA:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_CUDA))
        return {};
      return {AV_PIX_FMT_CUDA, AV_HWDEVICE_TYPE_CUDA};
    case AV_PIX_FMT_QSV:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_QSV))
        return {};
      return {AV_PIX_FMT_QSV, AV_HWDEVICE_TYPE_QSV};
    case AV_PIX_FMT_VULKAN:
      if(!hardwareDecoderIsAvailable(AV_PIX_FMT_VULKAN))
        return {};
      return {AV_PIX_FMT_VULKAN, AV_HWDEVICE_TYPE_VULKAN};
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
    case AV_CODEC_ID_PRORES:
      return true;
    default:
      return false;
  }
}
/// Maps e.g. "h264" + CUDA -> "h264_cuvid". Returns generic name for hw_device_ctx backends.
inline std::string hwCodecName(const char* codec_name, AVHWDeviceType device)
{
  std::string name{codec_name};
  switch(device)
  {
    case AV_HWDEVICE_TYPE_CUDA:
      return name + "_cuvid";
    case AV_HWDEVICE_TYPE_QSV:
      return name + "_qsv";
    case AV_HWDEVICE_TYPE_VDPAU:
      return name + "_vdpau";
    case AV_HWDEVICE_TYPE_VAAPI:
      return name;
    case AV_HWDEVICE_TYPE_DRM:
      return name + "_v4l2m2m";
    case AV_HWDEVICE_TYPE_DXVA2:
    case AV_HWDEVICE_TYPE_D3D11VA:
#if defined(_WIN32) && LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
    case AV_HWDEVICE_TYPE_D3D12VA:
#endif
    case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
    case AV_HWDEVICE_TYPE_VULKAN:
      return name;
    default:
      return {};
  }
}

/// Codecs that FFmpeg decodes via Vulkan compute shaders (not Vulkan Video).
/// These are unstable on some drivers (Intel ANV) and should not use Vulkan HW accel.
inline bool isVulkanComputeCodec(AVCodecID id)
{
  switch(id)
  {
    case AV_CODEC_ID_PRORES:
    case AV_CODEC_ID_FFV1:
      return true;
    default:
      return false;
  }
}

/// Checks dedicated decoder name or avcodec_get_hw_config for codec+format support.
inline bool codecSupportsHWPixelFormat(
    AVCodecID codec_id, AVPixelFormat pix_fmt, uint32_t gpuVendorId = 0)
{
  (void)gpuVendorId;

  auto hwInfo = ffmpegHardwareDecodingFormats(pix_fmt);
  if(hwInfo.device == AV_HWDEVICE_TYPE_NONE)
    return false;

  const AVCodec* codec = avcodec_find_decoder(codec_id);
  if(!codec)
    return false;

  auto dedicated = hwCodecName(codec->name, hwInfo.device);
  if(!dedicated.empty() && dedicated != codec->name)
  {
    if(avcodec_find_decoder_by_name(dedicated.c_str()))
      return true;
    return false;
  }

  for(int i = 0;; i++)
  {
    const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
    if(!config)
      break;
    if(config->pix_fmt == pix_fmt
       && (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))
      return true;
  }
  return false;
}

/// PCI vendor IDs (from QRhi::driverInfo().vendorId)
namespace GpuVendor
{
inline constexpr uint32_t NVIDIA = 0x10DE;
inline constexpr uint32_t Intel = 0x8086;
inline constexpr uint32_t AMD = 0x1002;
inline constexpr uint32_t Apple = 0x106B;
inline constexpr uint32_t Broadcom = 0x14E4; // RPi VideoCore
inline constexpr uint32_t ARM = 0x13B5;      // Mali
inline constexpr uint32_t Qualcomm = 0x5143;
inline constexpr uint32_t Samsung = 0x144D;
}

/// Returns all viable HW accel formats for the given graphics API, codec, and
/// GPU vendor, in priority order. The caller can iterate and try each until one
/// succeeds at runtime.
/// graphicsApi: 0=Null, 1=OpenGL, 2=Vulkan, 3=D3D11, 4=Metal, 5=D3D12
inline std::vector<AVPixelFormat> selectHardwareAccelerations(
    int graphicsApi, AVCodecID codec_id, uint32_t gpuVendorId = 0)
{
  struct Candidate { AVPixelFormat fmt; };
  std::vector<Candidate> candidates;

  switch(graphicsApi)
  {
#if defined(_WIN32)
    case 3: // D3D11
      candidates = {{AV_PIX_FMT_D3D11}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
      break;
    case 5: // D3D12
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
      candidates = {{AV_PIX_FMT_D3D12}, {AV_PIX_FMT_D3D11}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
#else
      candidates = {{AV_PIX_FMT_D3D11}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
#endif
      break;
#endif
#if defined(__APPLE__)
    case 4: // Metal
      candidates = {{AV_PIX_FMT_VIDEOTOOLBOX}};
      break;
#endif
    case 2: // Vulkan
    {
#if defined(__linux__)
      if(gpuVendorId == GpuVendor::NVIDIA)
      {
        candidates = {{AV_PIX_FMT_VULKAN}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_VAAPI}, {AV_PIX_FMT_QSV}};
      }
      else if(gpuVendorId == GpuVendor::Intel)
      {
        candidates = {{AV_PIX_FMT_VAAPI}, {AV_PIX_FMT_VULKAN}, {AV_PIX_FMT_QSV}};
      }
      else if(gpuVendorId == GpuVendor::AMD)
      {
        candidates = {{AV_PIX_FMT_VAAPI}, {AV_PIX_FMT_VULKAN}};
      }
      else if(gpuVendorId == GpuVendor::Broadcom
              || gpuVendorId == GpuVendor::ARM
              || gpuVendorId == GpuVendor::Qualcomm)
      {
        candidates = {{AV_PIX_FMT_DRM_PRIME}, {AV_PIX_FMT_VULKAN}};
      }
      else
      {
        candidates = {{AV_PIX_FMT_VAAPI}, {AV_PIX_FMT_VULKAN}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
      }
#elif defined(_WIN32)
      candidates = {
          {AV_PIX_FMT_VULKAN}, {AV_PIX_FMT_D3D11},
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
          {AV_PIX_FMT_D3D12},
#endif
          {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
#elif defined(__APPLE__)
      candidates = {{AV_PIX_FMT_VIDEOTOOLBOX}};
#else
      candidates = {{AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
#endif
      break;
    }
    case 1: // OpenGL
    {
#if defined(__linux__)
      if(gpuVendorId == GpuVendor::Broadcom
         || gpuVendorId == GpuVendor::ARM
         || gpuVendorId == GpuVendor::Qualcomm)
      {
        candidates = {{AV_PIX_FMT_DRM_PRIME}};
      }
      else
      {
        candidates = {{AV_PIX_FMT_VAAPI}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
      }
#elif defined(_WIN32)
      candidates = {{AV_PIX_FMT_D3D11}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
#elif defined(__APPLE__)
      candidates = {{AV_PIX_FMT_VIDEOTOOLBOX}};
#endif
      break;
    }
    default:
    {
#if defined(__linux__)
#if defined(__arm__) || defined(__aarch64__)
      candidates = {{AV_PIX_FMT_DRM_PRIME}, {AV_PIX_FMT_VAAPI}};
#else
      candidates = {{AV_PIX_FMT_VAAPI}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
#endif
#elif defined(_WIN32)
      candidates = {{AV_PIX_FMT_D3D11}, {AV_PIX_FMT_CUDA}, {AV_PIX_FMT_QSV}};
#elif defined(__APPLE__)
      candidates = {{AV_PIX_FMT_VIDEOTOOLBOX}};
#endif
      break;
    }
  }

  std::vector<AVPixelFormat> result;
  for(auto& c : candidates)
  {
    if(hardwareDecoderIsAvailable(c.fmt)
       && codecSupportsHWPixelFormat(codec_id, c.fmt, gpuVendorId))
      result.push_back(c.fmt);
  }
  return result;
}

/// Picks the best HW accel for the given graphics API, codec, and GPU vendor.
inline AVPixelFormat selectHardwareAcceleration(
    int graphicsApi, AVCodecID codec_id, uint32_t gpuVendorId = 0)
{
  auto fmts = selectHardwareAccelerations(graphicsApi, codec_id, gpuVendorId);
  return fmts.empty() ? AV_PIX_FMT_NONE : fmts.front();
}

#endif
}

#endif
