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
    // The X-variants are the same bytes with the spare channel undefined;
    // FFmpeg spells those out as its 0-prefixed/suffixed forms, so they map
    // exactly rather than aliasing onto the alpha-bearing format.
    case V::BGRX8:     return AV_PIX_FMT_BGR0;
    case V::RGBX8:     return AV_PIX_FMT_RGB0;
    case V::XRGB8:     return AV_PIX_FMT_0RGB;
    case V::XBGR8:     return AV_PIX_FMT_0BGR;

    // -- packed sub-byte RGB --
    case V::RGB565:    return AV_PIX_FMT_RGB565LE;
    case V::RGB565BE:  return AV_PIX_FMT_RGB565BE;
    case V::RGB555:    return AV_PIX_FMT_RGB555LE;
    case V::RGB555BE:  return AV_PIX_FMT_RGB555BE;
    case V::RGB444:    return AV_PIX_FMT_RGB444LE;

    // -- high-precision RGB --
    case V::RGB48:     return AV_PIX_FMT_RGB48LE;  // AJA 48BIT_RGB is 16-bit LE
    case V::RGBA16:    return AV_PIX_FMT_RGBA64LE;

    // -- packed 8-bit YUV 4:2:2 --
    case V::UYVY422:   return AV_PIX_FMT_UYVY422;
    case V::YUYV422:   return AV_PIX_FMT_YUYV422;
    case V::YVYU422:   return AV_PIX_FMT_YVYU422;

    // -- planar / semi-planar YUV --
    case V::NV12:      return AV_PIX_FMT_NV12;
    case V::NV21:      return AV_PIX_FMT_NV21;
    case V::NV16:      return AV_PIX_FMT_NV16;
    case V::NV24:      return AV_PIX_FMT_NV24;
    case V::NV42:      return AV_PIX_FMT_NV42;
    case V::P010:      return AV_PIX_FMT_P010LE;
    case V::P210:      return AV_PIX_FMT_P210LE;
    case V::YUV420P:   return AV_PIX_FMT_YUV420P;
    case V::YUV420P10: return AV_PIX_FMT_YUV420P10LE;
    case V::YUV422P:   return AV_PIX_FMT_YUV422P;
    case V::YUV422P10: return AV_PIX_FMT_YUV422P10LE;
    case V::YUV422P12: return AV_PIX_FMT_YUV422P12LE;
    case V::YUV422P16: return AV_PIX_FMT_YUV422P16LE;
    case V::YUV411P:   return AV_PIX_FMT_YUV411P;
    case V::YUV410P:   return AV_PIX_FMT_YUV410P;
    case V::UYYVYY411: return AV_PIX_FMT_UYYVYY411;
    case V::P216:      return AV_PIX_FMT_P216LE;
    case V::P416:      return AV_PIX_FMT_P416LE;
    case V::Y210:      return AV_PIX_FMT_Y210LE;
    case V::YUVA444P:  return AV_PIX_FMT_YUVA444P;
    case V::YUV444P:   return AV_PIX_FMT_YUV444P;
    case V::YUV444P10: return AV_PIX_FMT_YUV444P10LE;
    case V::YUV444P12: return AV_PIX_FMT_YUV444P12LE;

    // -- packed 4:4:4 YUV with/without alpha --
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 36, 100)
    case V::VUYA:      return AV_PIX_FMT_VUYA;
    case V::VUYX:      return AV_PIX_FMT_VUYX;
    case V::XV30:      return AV_PIX_FMT_XV30LE;
#else
    case V::VUYA:
    case V::VUYX:
    case V::XV30:
      return AV_PIX_FMT_NONE;
#endif
    case V::AYUV64:    return AV_PIX_FMT_AYUV64LE;

    // -- grey --
    case V::Mono8:     return AV_PIX_FMT_GRAY8;
    case V::Mono10:    return AV_PIX_FMT_GRAY10LE;
    case V::Mono12:    return AV_PIX_FMT_GRAY12LE;
    case V::Mono16:    return AV_PIX_FMT_GRAY16LE;
    case V::Mono16BE:  return AV_PIX_FMT_GRAY16BE;

    // -- Bayer: the CFA order is part of the format identity --
    case V::BayerBGGR8:  return AV_PIX_FMT_BAYER_BGGR8;
    case V::BayerGBRG8:  return AV_PIX_FMT_BAYER_GBRG8;
    case V::BayerGRBG8:  return AV_PIX_FMT_BAYER_GRBG8;
    case V::BayerRGGB8:  return AV_PIX_FMT_BAYER_RGGB8;
    case V::BayerBGGR16: return AV_PIX_FMT_BAYER_BGGR16LE;
    case V::BayerRGGB16: return AV_PIX_FMT_BAYER_RGGB16LE;

    // -- wire-only: no AVPixelFormat (FFmpeg models these as codecs) --
    case V::V210:      // AV_CODEC_ID_V210
    case V::V216:      // AV_CODEC_ID_V210X
    case V::R210:      // AV_CODEC_ID_R210 (big-endian, R high)
    case V::RGB10:     // AJA NTV2_FBF_10BIT_RGB (little-endian, B high)
    case V::R12B:      // AV_CODEC_ID_DPX
    case V::R12L:
    case V::ARGB10:    // 4x10 bits packed into 5 bytes, alpha included
    case V::DPX10:
    case V::DPX10LE:
    case V::RGB12P:
    case V::VYUY422:   // no AV_PIX_FMT_VYUY
    // -- no clean/unambiguous AVPixelFormat twin --
    case V::RGBA16F:   // no packed half-float RGBA pixfmt
    case V::RGBA32F:   // no packed float RGBA pixfmt (GBRPF32 is planar)
    // PFNC aliases of the RGGB order; see the table. Unbridged so that no two
    // enumerators claim the same AVPixelFormat.
    case V::BayerRG8:
    case V::BayerRG12:
    // FFmpeg has no plane-swapped planar twin: YV12 is expressed as yuv420p
    // with the U and V pointers exchanged, so returning yuv420p here would
    // silently swap chroma -- the exact bug this bridge exists to avoid.
    case V::YVU420P:
    case V::YVU410P:
    case V::YVU422P:
    case V::NV61:      // no AV_PIX_FMT_NV61 (NV16 with the pair exchanged)
    case V::Y216:      // no AV_PIX_FMT_Y216 in any supported libav
    // Packed 4:4:4 orders FFmpeg does not spell: it has VUYA/VUYX/UYVA only.
    case V::AYUV:
    case V::XYUV:
    case V::YUVA:
    case V::YUVX:
    // AV_PIX_FMT_RGB8/BGR8 are 2:3:3, not the 3:3:2 of V4L2's RGB332.
    case V::RGB332:
    // 16-bit containers whose spare bit(s) FFmpeg treats as padding rather
    // than alpha, so the alpha-bearing variants have no exact twin.
    case V::ARGB1555:
    case V::ARGB4444:
    case V::AYUV4444:
    case V::AYUV1555:
    case V::YUV565:
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
    case AV_PIX_FMT_BGR0:        return V::BGRX8;
    case AV_PIX_FMT_RGB0:        return V::RGBX8;
    case AV_PIX_FMT_0RGB:        return V::XRGB8;
    case AV_PIX_FMT_0BGR:        return V::XBGR8;
    case AV_PIX_FMT_RGB565LE:    return V::RGB565;
    case AV_PIX_FMT_RGB565BE:    return V::RGB565BE;
    case AV_PIX_FMT_RGB555LE:    return V::RGB555;
    case AV_PIX_FMT_RGB555BE:    return V::RGB555BE;
    case AV_PIX_FMT_RGB444LE:    return V::RGB444;
    case AV_PIX_FMT_RGB48LE:     return V::RGB48;
    case AV_PIX_FMT_RGBA64LE:    return V::RGBA16;
    case AV_PIX_FMT_UYVY422:     return V::UYVY422;
    case AV_PIX_FMT_YUYV422:     return V::YUYV422;
    case AV_PIX_FMT_YVYU422:     return V::YVYU422;
    case AV_PIX_FMT_NV12:        return V::NV12;
    case AV_PIX_FMT_NV21:        return V::NV21;
    case AV_PIX_FMT_NV16:        return V::NV16;
    case AV_PIX_FMT_NV24:        return V::NV24;
    case AV_PIX_FMT_NV42:        return V::NV42;
    case AV_PIX_FMT_P010LE:      return V::P010;
    case AV_PIX_FMT_P210LE:      return V::P210;
    case AV_PIX_FMT_YUV420P:     return V::YUV420P;
    case AV_PIX_FMT_YUV420P10LE: return V::YUV420P10;
    case AV_PIX_FMT_YUV422P:     return V::YUV422P;
    case AV_PIX_FMT_YUV422P10LE: return V::YUV422P10;
    case AV_PIX_FMT_YUV422P12LE: return V::YUV422P12;
    case AV_PIX_FMT_YUV422P16LE: return V::YUV422P16;
    case AV_PIX_FMT_YUV411P:     return V::YUV411P;
    case AV_PIX_FMT_YUV410P:     return V::YUV410P;
    case AV_PIX_FMT_UYYVYY411:   return V::UYYVYY411;
    case AV_PIX_FMT_P216LE:      return V::P216;
    case AV_PIX_FMT_P416LE:      return V::P416;
    case AV_PIX_FMT_Y210LE:      return V::Y210;
    case AV_PIX_FMT_YUVA444P:    return V::YUVA444P;
    case AV_PIX_FMT_YUV444P:     return V::YUV444P;
    case AV_PIX_FMT_YUV444P10LE: return V::YUV444P10;
    case AV_PIX_FMT_YUV444P12LE: return V::YUV444P12;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 36, 100)
    case AV_PIX_FMT_VUYA:        return V::VUYA;
    case AV_PIX_FMT_VUYX:        return V::VUYX;
    case AV_PIX_FMT_XV30LE:      return V::XV30;
#endif
    case AV_PIX_FMT_AYUV64LE:    return V::AYUV64;
    case AV_PIX_FMT_GRAY8:       return V::Mono8;
    case AV_PIX_FMT_GRAY10LE:    return V::Mono10;
    case AV_PIX_FMT_GRAY12LE:    return V::Mono12;
    case AV_PIX_FMT_GRAY16LE:    return V::Mono16;
    case AV_PIX_FMT_GRAY16BE:    return V::Mono16BE;
    case AV_PIX_FMT_BAYER_BGGR8:    return V::BayerBGGR8;
    case AV_PIX_FMT_BAYER_GBRG8:    return V::BayerGBRG8;
    case AV_PIX_FMT_BAYER_GRBG8:    return V::BayerGRBG8;
    case AV_PIX_FMT_BAYER_RGGB8:    return V::BayerRGGB8;
    case AV_PIX_FMT_BAYER_BGGR16LE: return V::BayerBGGR16;
    case AV_PIX_FMT_BAYER_RGGB16LE: return V::BayerRGGB16;
    default:                     return V::Unknown;
  }
}

} // namespace score::gfx::interop
