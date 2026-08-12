#include <Gfx/Graph/interop/DrmPixelFormat.hpp>

namespace score::gfx::interop
{
namespace
{
constexpr auto drmFcc = drmPixelFourcc;

// A DRM fourcc reads in machine-word order, so the memory byte order is the
// reverse of the name. The comments give the memory order, which is what a
// VideoPixelFormat names.
constexpr uint32_t kArgb8888 = drmFcc('A', 'R', '2', '4');     // B,G,R,A
constexpr uint32_t kAbgr8888 = drmFcc('A', 'B', '2', '4');     // R,G,B,A
constexpr uint32_t kXrgb8888 = drmFcc('X', 'R', '2', '4');     // B,G,R,X
constexpr uint32_t kXbgr8888 = drmFcc('X', 'B', '2', '4');     // R,G,B,X
constexpr uint32_t kRgba8888 = drmFcc('R', 'A', '2', '4');     // A,B,G,R
constexpr uint32_t kBgra8888 = drmFcc('B', 'A', '2', '4');     // A,R,G,B
constexpr uint32_t kRgb888 = drmFcc('R', 'G', '2', '4');       // B,G,R
constexpr uint32_t kBgr888 = drmFcc('B', 'G', '2', '4');       // R,G,B
constexpr uint32_t kRgb565 = drmFcc('R', 'G', '1', '6');
constexpr uint32_t kArgb2101010 = drmFcc('A', 'R', '3', '0');  // 2R10G10B10, LE word
constexpr uint32_t kAbgr2101010 = drmFcc('A', 'B', '3', '0');  // 2B10G10R10, LE word
constexpr uint32_t kAbgr16161616f = drmFcc('A', 'B', '4', 'H');
constexpr uint32_t kNv12 = drmFcc('N', 'V', '1', '2');
constexpr uint32_t kNv21 = drmFcc('N', 'V', '2', '1');
constexpr uint32_t kNv16 = drmFcc('N', 'V', '1', '6');
constexpr uint32_t kNv61 = drmFcc('N', 'V', '6', '1');
constexpr uint32_t kNv24 = drmFcc('N', 'V', '2', '4');
constexpr uint32_t kNv42 = drmFcc('N', 'V', '4', '2');
constexpr uint32_t kP010 = drmFcc('P', '0', '1', '0');
constexpr uint32_t kP210 = drmFcc('P', '2', '1', '0');
constexpr uint32_t kP410 = drmFcc('P', '4', '1', '0');
constexpr uint32_t kYuv420 = drmFcc('Y', 'U', '1', '2');
constexpr uint32_t kYvu420 = drmFcc('Y', 'V', '1', '2');
constexpr uint32_t kYuv422 = drmFcc('Y', 'U', '1', '6');
constexpr uint32_t kYvu422 = drmFcc('Y', 'V', '1', '6');
constexpr uint32_t kYuv444 = drmFcc('Y', 'U', '2', '4');
constexpr uint32_t kYuyv = drmFcc('Y', 'U', 'Y', 'V');
constexpr uint32_t kYvyu = drmFcc('Y', 'V', 'Y', 'U');
constexpr uint32_t kUyvy = drmFcc('U', 'Y', 'V', 'Y');
constexpr uint32_t kVyuy = drmFcc('V', 'Y', 'U', 'Y');
constexpr uint32_t kR8 = drmFcc('R', '8', ' ', ' ');
constexpr uint32_t kR16 = drmFcc('R', '1', '6', ' ');
} // namespace

VideoPixelFormat fromDrmFourcc(uint32_t fourcc) noexcept
{
  using V = VideoPixelFormat;
  switch(fourcc)
  {
    case kArgb8888:      return V::BGRA8;
    case kAbgr8888:      return V::RGBA8;
    case kXrgb8888:      return V::BGRX8;
    case kXbgr8888:      return V::RGBX8;
    case kRgba8888:      return V::ABGR8;
    case kBgra8888:      return V::ARGB8;
    case kRgb888:        return V::BGR24;
    case kBgr888:        return V::RGB24;
    case kRgb565:        return V::RGB565;
    case kArgb2101010:   return V::X2RGB10;
    case kAbgr2101010:   return V::X2BGR10;
    case kAbgr16161616f: return V::RGBA16F;
    case kNv12:          return V::NV12;
    case kNv21:          return V::NV21;
    case kNv16:          return V::NV16;
    case kNv61:          return V::NV61;
    case kNv24:          return V::NV24;
    case kNv42:          return V::NV42;
    case kP010:          return V::P010;
    case kP210:          return V::P210;
    case kP410:          return V::P416;
    case kYuv420:        return V::YUV420P;
    case kYvu420:        return V::YVU420P;
    case kYuv422:        return V::YUV422P;
    case kYvu422:        return V::YVU422P;
    case kYuv444:        return V::YUV444P;
    case kYuyv:          return V::YUYV422;
    case kYvyu:          return V::YVYU422;
    case kUyvy:          return V::UYVY422;
    case kVyuy:          return V::VYUY422;
    case kR8:            return V::Mono8;
    case kR16:           return V::Mono16;
    default:             return V::Unknown;
  }
}

uint32_t toDrmFourcc(VideoPixelFormat f) noexcept
{
  using V = VideoPixelFormat;
  switch(f)
  {
    case V::BGRA8:   return kArgb8888;
    case V::RGBA8:   return kAbgr8888;
    case V::BGRX8:   return kXrgb8888;
    case V::RGBX8:   return kXbgr8888;
    case V::ABGR8:   return kRgba8888;
    case V::ARGB8:   return kBgra8888;
    case V::BGR24:   return kRgb888;
    case V::RGB24:   return kBgr888;
    case V::RGB565:  return kRgb565;
    case V::X2RGB10: return kArgb2101010;
    case V::X2BGR10: return kAbgr2101010;
    case V::RGBA16F: return kAbgr16161616f;
    case V::NV12:    return kNv12;
    case V::NV21:    return kNv21;
    case V::NV16:    return kNv16;
    case V::NV61:    return kNv61;
    case V::NV24:    return kNv24;
    case V::NV42:    return kNv42;
    case V::P010:    return kP010;
    case V::P210:    return kP210;
    case V::P416:    return kP410;
    case V::YUV420P: return kYuv420;
    case V::YVU420P: return kYvu420;
    case V::YUV422P: return kYuv422;
    case V::YVU422P: return kYvu422;
    case V::YUV444P: return kYuv444;
    case V::YUYV422: return kYuyv;
    case V::YVYU422: return kYvyu;
    case V::UYVY422: return kUyvy;
    case V::VYUY422: return kVyuy;
    case V::Mono8:   return kR8;
    case V::Mono16:  return kR16;
    // A colour-filter-array frame is one sample per pixel, so on the wire it is
    // byte-identical to greyscale of the same depth -- the CFA order lives in
    // the score-side format, not in the DRM layout. Only the to-DRM direction
    // is given: coming back, R8/R16 stay Mono8/Mono16, since the fourcc cannot
    // say which of the five enumerators sharing it was meant.
    case V::BayerRGGB8:
    case V::BayerBGGR8:
    case V::BayerGRBG8:
    case V::BayerGBRG8:
    case V::BayerRG8:   return kR8;
    case V::BayerRGGB16:
    case V::BayerBGGR16:
    case V::BayerRGGB10:
    case V::BayerBGGR10:
    case V::BayerGRBG10:
    case V::BayerGBRG10:
    case V::BayerRG12:  return kR16;
    default:         return 0;
  }
}

} // namespace score::gfx::interop
