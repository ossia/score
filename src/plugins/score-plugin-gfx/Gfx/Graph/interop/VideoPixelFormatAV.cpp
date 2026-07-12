#include <Gfx/Graph/interop/VideoPixelFormatAV.hpp>

namespace score::gfx::interop
{

AVPixelFormat toAVPixelFormat(VideoPixelFormat f) noexcept
{
  using V = VideoPixelFormat;
  switch(f)
  {
    // -- packed 8-bit RGB --
    case V::BGRA8:     return AV_PIX_FMT_BGRA;
    case V::RGBA8:     return AV_PIX_FMT_RGBA;
    case V::ARGB8:     return AV_PIX_FMT_ARGB;
    case V::ABGR8:     return AV_PIX_FMT_ABGR;
    case V::RGB24:     return AV_PIX_FMT_RGB24;
    case V::BGR24:     return AV_PIX_FMT_BGR24;

    // -- high-precision RGB --
    case V::RGB48:     return AV_PIX_FMT_RGB48LE;  // AJA 48BIT_RGB is 16-bit LE
    case V::RGBA16:    return AV_PIX_FMT_RGBA64LE;

    // -- packed 8-bit YUV 4:2:2 --
    case V::UYVY422:   return AV_PIX_FMT_UYVY422;
    case V::YUYV422:   return AV_PIX_FMT_YUYV422;
    case V::YVYU422:   return AV_PIX_FMT_YVYU422;

    // -- planar / semi-planar YUV --
    case V::NV12:      return AV_PIX_FMT_NV12;
    case V::P010:      return AV_PIX_FMT_P010LE;
    case V::P210:      return AV_PIX_FMT_P210LE;
    case V::YUV420P:   return AV_PIX_FMT_YUV420P;
    case V::YUV420P10: return AV_PIX_FMT_YUV420P10LE;
    case V::YUV422P:   return AV_PIX_FMT_YUV422P;
    case V::YUV422P10: return AV_PIX_FMT_YUV422P10LE;
    case V::YUV444P:   return AV_PIX_FMT_YUV444P;
    case V::YUV444P10: return AV_PIX_FMT_YUV444P10LE;
    case V::YUV444P12: return AV_PIX_FMT_YUV444P12LE;

    // -- grey --
    case V::Mono8:     return AV_PIX_FMT_GRAY8;
    case V::Mono10:    return AV_PIX_FMT_GRAY10LE;
    case V::Mono12:    return AV_PIX_FMT_GRAY12LE;
    case V::Mono16:    return AV_PIX_FMT_GRAY16LE;

    // -- wire-only: no AVPixelFormat (FFmpeg models these as codecs) --
    case V::V210:      // AV_CODEC_ID_V210
    case V::V216:      // AV_CODEC_ID_V210X
    case V::R210:      // AV_CODEC_ID_R210 (big-endian, R high)
    case V::RGB10:     // AJA NTV2_FBF_10BIT_RGB (little-endian, B high)
    case V::R12B:      // AV_CODEC_ID_DPX
    case V::R12L:
    case V::ARGB10:    // A2R10G10B10 packed
    case V::DPX10:
    case V::DPX10LE:
    case V::RGB12P:
    case V::VYUY422:   // no AV_PIX_FMT_VYUY
    // -- no clean/unambiguous AVPixelFormat twin --
    case V::RGBA16F:   // no packed half-float RGBA pixfmt
    case V::RGBA32F:   // no packed float RGBA pixfmt (GBRPF32 is planar)
    case V::BayerRG8:  // Bayer order differs per sensor; keep explicit
    case V::BayerRG12:
    case V::Unknown:
      return AV_PIX_FMT_NONE;
  }
  return AV_PIX_FMT_NONE;
}

VideoPixelFormat fromAVPixelFormat(AVPixelFormat f) noexcept
{
  using V = VideoPixelFormat;
  switch(f)
  {
    case AV_PIX_FMT_BGRA:        return V::BGRA8;
    case AV_PIX_FMT_RGBA:        return V::RGBA8;
    case AV_PIX_FMT_ARGB:        return V::ARGB8;
    case AV_PIX_FMT_ABGR:        return V::ABGR8;
    case AV_PIX_FMT_RGB24:       return V::RGB24;
    case AV_PIX_FMT_BGR24:       return V::BGR24;
    case AV_PIX_FMT_RGB48LE:     return V::RGB48;
    case AV_PIX_FMT_RGBA64LE:    return V::RGBA16;
    case AV_PIX_FMT_UYVY422:     return V::UYVY422;
    case AV_PIX_FMT_YUYV422:     return V::YUYV422;
    case AV_PIX_FMT_YVYU422:     return V::YVYU422;
    case AV_PIX_FMT_NV12:        return V::NV12;
    case AV_PIX_FMT_P010LE:      return V::P010;
    case AV_PIX_FMT_P210LE:      return V::P210;
    case AV_PIX_FMT_YUV420P:     return V::YUV420P;
    case AV_PIX_FMT_YUV420P10LE: return V::YUV420P10;
    case AV_PIX_FMT_YUV422P:     return V::YUV422P;
    case AV_PIX_FMT_YUV422P10LE: return V::YUV422P10;
    case AV_PIX_FMT_YUV444P:     return V::YUV444P;
    case AV_PIX_FMT_YUV444P10LE: return V::YUV444P10;
    case AV_PIX_FMT_YUV444P12LE: return V::YUV444P12;
    case AV_PIX_FMT_GRAY8:       return V::Mono8;
    case AV_PIX_FMT_GRAY10LE:    return V::Mono10;
    case AV_PIX_FMT_GRAY12LE:    return V::Mono12;
    case AV_PIX_FMT_GRAY16LE:    return V::Mono16;
    default:                     return V::Unknown;
  }
}

} // namespace score::gfx::interop
