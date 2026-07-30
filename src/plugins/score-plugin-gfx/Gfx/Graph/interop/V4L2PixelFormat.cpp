#include <Gfx/Graph/interop/V4L2PixelFormat.hpp>

#if defined(__linux__)
#include <linux/videodev2.h>

namespace score::gfx::interop
{

VideoPixelFormat fromV4L2PixelFormat(uint32_t fourcc) noexcept
{
  using V = VideoPixelFormat;
  switch(fourcc)
  {
    // -- packed YUV 4:2:2 --
    case V4L2_PIX_FMT_UYVY:    return V::UYVY422;
    case V4L2_PIX_FMT_YUYV:    return V::YUYV422;
    case V4L2_PIX_FMT_YVYU:    return V::YVYU422;
    case V4L2_PIX_FMT_VYUY:    return V::VYUY422;

    // -- semi-planar --
    case V4L2_PIX_FMT_NV12:    return V::NV12;
    case V4L2_PIX_FMT_NV21:    return V::NV21;
    case V4L2_PIX_FMT_NV16:    return V::NV16;
    case V4L2_PIX_FMT_NV61:    return V::NV61;
    case V4L2_PIX_FMT_NV24:    return V::NV24;
    case V4L2_PIX_FMT_NV42:    return V::NV42;

    // -- fully planar. The V-first orders are distinct layouts, not the
    //    U-first ones with a flag: reading YV12 as I420 swaps chroma.
    case V4L2_PIX_FMT_YUV420:  return V::YUV420P;
    case V4L2_PIX_FMT_YVU420:  return V::YVU420P;
    case V4L2_PIX_FMT_YUV422P: return V::YUV422P;
    case V4L2_PIX_FMT_YUV411P: return V::YUV411P;
    case V4L2_PIX_FMT_YUV410:  return V::YUV410P;
    case V4L2_PIX_FMT_YVU410:  return V::YVU410P;
#ifdef V4L2_PIX_FMT_YUV444M
    case V4L2_PIX_FMT_YUV444M: return V::YUV444P;
#endif

    // -- packed 4:4:4 with or without alpha --
    case V4L2_PIX_FMT_VUYA32:  return V::VUYA;
    case V4L2_PIX_FMT_VUYX32:  return V::VUYX;
    case V4L2_PIX_FMT_AYUV32:  return V::AYUV;
    case V4L2_PIX_FMT_XYUV32:  return V::XYUV;
    case V4L2_PIX_FMT_YUVA32:  return V::YUVA;
    case V4L2_PIX_FMT_YUVX32:  return V::YUVX;

    // -- packed 16-bit YUV --
    case V4L2_PIX_FMT_YUV444:  return V::AYUV4444;
    case V4L2_PIX_FMT_YUV555:  return V::AYUV1555;
    case V4L2_PIX_FMT_YUV565:  return V::YUV565;

    // -- packed 32-bit RGB. The fourcc names the memory byte order.
    case V4L2_PIX_FMT_ARGB32:  return V::ARGB8;
    case V4L2_PIX_FMT_XRGB32:  return V::XRGB8;
    case V4L2_PIX_FMT_ABGR32:  return V::BGRA8;
    case V4L2_PIX_FMT_XBGR32:  return V::BGRX8;
#ifdef V4L2_PIX_FMT_RGBA32
    case V4L2_PIX_FMT_RGBA32:  return V::RGBA8;
    case V4L2_PIX_FMT_RGBX32:  return V::RGBX8;
    case V4L2_PIX_FMT_BGRA32:  return V::ABGR8;
    case V4L2_PIX_FMT_BGRX32:  return V::XBGR8;
#endif
    // Deprecated aliases. The kernel documents them as the X-variants with
    // undefined alpha, which is also what libavdevice has long assumed.
    case V4L2_PIX_FMT_RGB32:   return V::XRGB8;
    case V4L2_PIX_FMT_BGR32:   return V::BGRX8;

    // -- packed 24-bit and sub-byte RGB --
    case V4L2_PIX_FMT_RGB24:   return V::RGB24;
    case V4L2_PIX_FMT_BGR24:   return V::BGR24;
    case V4L2_PIX_FMT_RGB332:  return V::RGB332;
    case V4L2_PIX_FMT_RGB565:  return V::RGB565;
    case V4L2_PIX_FMT_RGB565X: return V::RGB565BE;
    case V4L2_PIX_FMT_RGB555:  return V::RGB555;
    case V4L2_PIX_FMT_RGB555X: return V::RGB555BE;
    case V4L2_PIX_FMT_ARGB555: return V::ARGB1555;
    case V4L2_PIX_FMT_ARGB444: return V::ARGB4444;
    case V4L2_PIX_FMT_RGB444:  return V::RGB444;

    // -- single channel: greyscale sensors and depth streams. Z16 is depth,
    //    but its layout is a 16-bit single channel like any other.
    case V4L2_PIX_FMT_GREY:    return V::Mono8;
    case V4L2_PIX_FMT_Y10:     return V::Mono10;
    case V4L2_PIX_FMT_Y12:     return V::Mono12;
    case V4L2_PIX_FMT_Y16:     return V::Mono16;
    case V4L2_PIX_FMT_Y16_BE:  return V::Mono16BE;
#ifdef V4L2_PIX_FMT_Z16
    case V4L2_PIX_FMT_Z16:     return V::Mono16;
#endif

    // -- Bayer: the fourcc names the colour-filter-array order --
    case V4L2_PIX_FMT_SBGGR8:  return V::BayerBGGR8;
    case V4L2_PIX_FMT_SGBRG8:  return V::BayerGBRG8;
    case V4L2_PIX_FMT_SGRBG8:  return V::BayerGRBG8;
    case V4L2_PIX_FMT_SRGGB8:  return V::BayerRGGB8;
#ifdef V4L2_PIX_FMT_SBGGR16
    case V4L2_PIX_FMT_SBGGR16: return V::BayerBGGR16;
#endif
#ifdef V4L2_PIX_FMT_SRGGB16
    case V4L2_PIX_FMT_SRGGB16: return V::BayerRGGB16;
#endif

    default:                   return V::Unknown;
  }
}

uint32_t toV4L2PixelFormat(VideoPixelFormat f) noexcept
{
  using V = VideoPixelFormat;
  switch(f)
  {
    case V::UYVY422:    return V4L2_PIX_FMT_UYVY;
    case V::YUYV422:    return V4L2_PIX_FMT_YUYV;
    case V::YVYU422:    return V4L2_PIX_FMT_YVYU;
    case V::VYUY422:    return V4L2_PIX_FMT_VYUY;
    case V::NV12:       return V4L2_PIX_FMT_NV12;
    case V::NV21:       return V4L2_PIX_FMT_NV21;
    case V::NV16:       return V4L2_PIX_FMT_NV16;
    case V::NV61:       return V4L2_PIX_FMT_NV61;
    case V::NV24:       return V4L2_PIX_FMT_NV24;
    case V::NV42:       return V4L2_PIX_FMT_NV42;
    case V::YUV420P:    return V4L2_PIX_FMT_YUV420;
    case V::YVU420P:    return V4L2_PIX_FMT_YVU420;
    case V::YUV422P:    return V4L2_PIX_FMT_YUV422P;
    case V::YUV411P:    return V4L2_PIX_FMT_YUV411P;
    case V::YUV410P:    return V4L2_PIX_FMT_YUV410;
    case V::YVU410P:    return V4L2_PIX_FMT_YVU410;
    case V::VUYA:       return V4L2_PIX_FMT_VUYA32;
    case V::VUYX:       return V4L2_PIX_FMT_VUYX32;
    case V::AYUV:       return V4L2_PIX_FMT_AYUV32;
    case V::XYUV:       return V4L2_PIX_FMT_XYUV32;
    case V::YUVA:       return V4L2_PIX_FMT_YUVA32;
    case V::YUVX:       return V4L2_PIX_FMT_YUVX32;
    case V::AYUV4444:   return V4L2_PIX_FMT_YUV444;
    case V::AYUV1555:   return V4L2_PIX_FMT_YUV555;
    case V::YUV565:     return V4L2_PIX_FMT_YUV565;
    case V::ARGB8:      return V4L2_PIX_FMT_ARGB32;
    case V::XRGB8:      return V4L2_PIX_FMT_XRGB32;
    case V::BGRA8:      return V4L2_PIX_FMT_ABGR32;
    case V::BGRX8:      return V4L2_PIX_FMT_XBGR32;
#ifdef V4L2_PIX_FMT_RGBA32
    case V::RGBA8:      return V4L2_PIX_FMT_RGBA32;
    case V::RGBX8:      return V4L2_PIX_FMT_RGBX32;
    case V::ABGR8:      return V4L2_PIX_FMT_BGRA32;
    case V::XBGR8:      return V4L2_PIX_FMT_BGRX32;
#endif
    case V::RGB24:      return V4L2_PIX_FMT_RGB24;
    case V::BGR24:      return V4L2_PIX_FMT_BGR24;
    case V::RGB332:     return V4L2_PIX_FMT_RGB332;
    case V::RGB565:     return V4L2_PIX_FMT_RGB565;
    case V::RGB565BE:   return V4L2_PIX_FMT_RGB565X;
    case V::RGB555:     return V4L2_PIX_FMT_RGB555;
    case V::RGB555BE:   return V4L2_PIX_FMT_RGB555X;
    case V::ARGB1555:   return V4L2_PIX_FMT_ARGB555;
    case V::ARGB4444:   return V4L2_PIX_FMT_ARGB444;
    case V::RGB444:     return V4L2_PIX_FMT_RGB444;
    case V::Mono8:      return V4L2_PIX_FMT_GREY;
    case V::Mono10:     return V4L2_PIX_FMT_Y10;
    case V::Mono12:     return V4L2_PIX_FMT_Y12;
    case V::Mono16:     return V4L2_PIX_FMT_Y16;
    case V::Mono16BE:   return V4L2_PIX_FMT_Y16_BE;
    case V::BayerBGGR8: return V4L2_PIX_FMT_SBGGR8;
    case V::BayerGBRG8: return V4L2_PIX_FMT_SGBRG8;
    case V::BayerGRBG8: return V4L2_PIX_FMT_SGRBG8;
    case V::BayerRGGB8: return V4L2_PIX_FMT_SRGGB8;
#ifdef V4L2_PIX_FMT_SBGGR16
    case V::BayerBGGR16: return V4L2_PIX_FMT_SBGGR16;
#endif
#ifdef V4L2_PIX_FMT_SRGGB16
    case V::BayerRGGB16: return V4L2_PIX_FMT_SRGGB16;
#endif
    default:            return 0;
  }
}

} // namespace score::gfx::interop
#endif
