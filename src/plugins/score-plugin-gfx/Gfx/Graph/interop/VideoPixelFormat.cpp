#include "VideoPixelFormat.hpp"

namespace score::gfx::interop
{

namespace
{

constexpr VideoPixelFormatInfo kUnknown{
    "unknown", 0, 1, 1, 1, false, false, 256};

constexpr VideoPixelFormatInfo
makeRgb(const char* name, uint8_t bpp, uint16_t align = 256) noexcept
{
  return VideoPixelFormatInfo{name, bpp, 1, 1, 1, false, false, align};
}

constexpr VideoPixelFormatInfo makeYuv422Packed(const char* name,
                                                uint8_t bpp,
                                                uint16_t align = 256) noexcept
{
  return VideoPixelFormatInfo{name, bpp, 1, 2, 1, true, false, align};
}

constexpr VideoPixelFormatInfo makeNv12(const char* name, uint8_t bpp) noexcept
{
  return VideoPixelFormatInfo{name, bpp, 2, 2, 2, true, true, 256};
}

constexpr VideoPixelFormatInfo makeNv16(const char* name, uint8_t bpp) noexcept
{
  return VideoPixelFormatInfo{name, bpp, 2, 2, 1, true, true, 256};
}

constexpr VideoPixelFormatInfo
makePlanar(const char* name, uint8_t bpp, uint8_t hs, uint8_t vs) noexcept
{
  return VideoPixelFormatInfo{name, bpp, 3, hs, vs, true, true, 256};
}

constexpr VideoPixelFormatInfo makeMono(const char* name, uint8_t bpp) noexcept
{
  return VideoPixelFormatInfo{name, bpp, 1, 1, 1, false, false, 64};
}

} // namespace

const VideoPixelFormatInfo& formatInfo(VideoPixelFormat f) noexcept
{
  // The clang-friendly approach: declare each info in a switch and
  // return a reference to a static-local. Keeps the data localized
  // with each enum value.
  switch(f)
  {
    case VideoPixelFormat::BGRA8:
    {
      static constexpr auto i = makeRgb("BGRA8", 32);
      return i;
    }
    case VideoPixelFormat::RGBA8:
    {
      static constexpr auto i = makeRgb("RGBA8", 32);
      return i;
    }
    case VideoPixelFormat::ARGB8:
    {
      static constexpr auto i = makeRgb("ARGB8", 32);
      return i;
    }
    case VideoPixelFormat::ABGR8:
    {
      static constexpr auto i = makeRgb("ABGR8", 32);
      return i;
    }
    case VideoPixelFormat::RGB24:
    {
      static constexpr auto i = makeRgb("RGB24", 24, 64);
      return i;
    }
    case VideoPixelFormat::BGR24:
    {
      static constexpr auto i = makeRgb("BGR24", 24, 64);
      return i;
    }
    case VideoPixelFormat::R210:
    {
      static constexpr auto i = makeRgb("R210", 32, 256);
      return i;
    }
    case VideoPixelFormat::R12B:
    {
      static constexpr auto i = makeRgb("R12B", 36, 256);
      return i;
    }
    case VideoPixelFormat::R12L:
    {
      static constexpr auto i = makeRgb("R12L", 36, 256);
      return i;
    }
    case VideoPixelFormat::ARGB10:
    {
      static constexpr auto i = makeRgb("ARGB10", 32, 256);
      return i;
    }
    case VideoPixelFormat::DPX10:
    {
      static constexpr auto i = makeRgb("DPX10", 32, 256);
      return i;
    }
    case VideoPixelFormat::DPX10LE:
    {
      static constexpr auto i = makeRgb("DPX10LE", 32, 256);
      return i;
    }
    case VideoPixelFormat::RGB12P:
    {
      static constexpr auto i = makeRgb("RGB12P", 36, 256);
      return i;
    }
    case VideoPixelFormat::RGB48:
    {
      static constexpr auto i = makeRgb("RGB48", 48, 256);
      return i;
    }
    case VideoPixelFormat::UYVY422:
    {
      static constexpr auto i = makeYuv422Packed("UYVY422", 16);
      return i;
    }
    case VideoPixelFormat::YUYV422:
    {
      static constexpr auto i = makeYuv422Packed("YUYV422", 16);
      return i;
    }
    case VideoPixelFormat::YVYU422:
    {
      static constexpr auto i = makeYuv422Packed("YVYU422", 16);
      return i;
    }
    case VideoPixelFormat::VYUY422:
    {
      static constexpr auto i = makeYuv422Packed("VYUY422", 16);
      return i;
    }
    case VideoPixelFormat::V210:
    {
      // V210 effective bits-per-pixel: 16 bytes / 6 pixels = 21.33 bpp.
      // Round up to 22 for byte calc; the real stride formula is
      // separate in defaultStride().
      static constexpr auto i = makeYuv422Packed("V210", 22, 128);
      return i;
    }
    case VideoPixelFormat::V216:
    {
      static constexpr auto i = makeYuv422Packed("V216", 32);
      return i;
    }
    case VideoPixelFormat::NV12:
    {
      static constexpr auto i = makeNv12("NV12", 12);
      return i;
    }
    case VideoPixelFormat::P010:
    {
      static constexpr auto i = makeNv12("P010", 24);
      return i;
    }
    case VideoPixelFormat::P210:
    {
      static constexpr auto i = makeNv16("P210", 32);
      return i;
    }
    case VideoPixelFormat::YUV420P:
    {
      static constexpr auto i = makePlanar("YUV420P", 12, 2, 2);
      return i;
    }
    case VideoPixelFormat::YUV420P10:
    {
      static constexpr auto i = makePlanar("YUV420P10", 24, 2, 2);
      return i;
    }
    case VideoPixelFormat::YUV422P:
    {
      static constexpr auto i = makePlanar("YUV422P", 16, 2, 1);
      return i;
    }
    case VideoPixelFormat::YUV422P10:
    {
      static constexpr auto i = makePlanar("YUV422P10", 32, 2, 1);
      return i;
    }
    case VideoPixelFormat::YUV444P:
    {
      static constexpr auto i = makePlanar("YUV444P", 24, 1, 1);
      return i;
    }
    case VideoPixelFormat::YUV444P10:
    {
      static constexpr auto i = makePlanar("YUV444P10", 48, 1, 1);
      return i;
    }
    case VideoPixelFormat::YUV444P12:
    {
      static constexpr auto i = makePlanar("YUV444P12", 48, 1, 1);
      return i;
    }
    case VideoPixelFormat::RGBA16:
    {
      static constexpr auto i = makeRgb("RGBA16", 64);
      return i;
    }
    case VideoPixelFormat::RGBA16F:
    {
      static constexpr auto i = makeRgb("RGBA16F", 64);
      return i;
    }
    case VideoPixelFormat::RGBA32F:
    {
      static constexpr auto i = makeRgb("RGBA32F", 128);
      return i;
    }
    case VideoPixelFormat::Mono8:
    {
      static constexpr auto i = makeMono("Mono8", 8);
      return i;
    }
    case VideoPixelFormat::Mono10:
    {
      static constexpr auto i = makeMono("Mono10", 16);
      return i;
    }
    case VideoPixelFormat::Mono12:
    {
      static constexpr auto i = makeMono("Mono12", 16);
      return i;
    }
    case VideoPixelFormat::Mono16:
    {
      static constexpr auto i = makeMono("Mono16", 16);
      return i;
    }
    case VideoPixelFormat::BayerRG8:
    {
      static constexpr auto i = makeMono("BayerRG8", 8);
      return i;
    }
    case VideoPixelFormat::BayerRG12:
    {
      static constexpr auto i = makeMono("BayerRG12", 16);
      return i;
    }
    case VideoPixelFormat::Unknown:
      break;
  }
  return kUnknown;
}

const char* formatName(VideoPixelFormat f) noexcept
{
  return formatInfo(f).name;
}

std::size_t defaultStride(VideoPixelFormat f, uint32_t width) noexcept
{
  // V210 has a non-trivial stride formula per SMPTE 296M.
  if(f == VideoPixelFormat::V210)
    return alignUp(((std::size_t(width) + 47u) / 48u) * 128u, 128u);

  const auto& info = formatInfo(f);
  if(info.bitsPerPixel == 0)
    return 0;
  // Packed and semi-planar: stride = width * bpp/8 for the primary plane.
  const std::size_t bytesPerRow
      = (std::size_t(width) * info.bitsPerPixel + 7u) / 8u;
  return alignUp(bytesPerRow, info.defaultStrideAlignment);
}

std::size_t bytesPerFrame(
    VideoPixelFormat f, uint32_t width, uint32_t height) noexcept
{
  const auto& info = formatInfo(f);
  if(info.bitsPerPixel == 0 || width == 0 || height == 0)
    return 0;

  if(!info.isPlanar)
    return defaultStride(f, width) * height;

  // Multi-plane: primary plane = width × height; chroma planes scaled
  // by subsampling factors. NV12-shape (semi-planar) gathers UV into a
  // single interleaved plane of half height.
  const std::size_t yBytes = defaultStride(f, width) * height;
  const std::size_t cWidth = width / info.horizontalSubsampling;
  const std::size_t cHeight = height / info.verticalSubsampling;
  if(info.planeCount == 2)
    return yBytes + defaultStride(f, uint32_t(cWidth * 2u)) * cHeight; // NV-style: interleaved UV
  if(info.planeCount == 3)
  {
    const std::size_t cBytes = defaultStride(f, uint32_t(cWidth)) * cHeight;
    return yBytes + 2u * cBytes;
  }
  return yBytes;
}

} // namespace score::gfx::interop
