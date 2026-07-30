#include "VideoPixelFormat.hpp"
#include <iterator>

namespace score::gfx::interop
{

namespace
{

constexpr VideoPixelFormatInfo kUnknown{};

// The descriptor array, generated from the one declarative table. Adding a
// format to the table adds it here, to the enum, and to what the unit test
// sweeps, all at once -- there is no second list to forget.
#define SCORE_VPF_INFO_ROW(                                                    \
    Name, Value, Model, Planes, Hsub, Vsub, BlockPixels, BlockBytes, Alpha,    \
    Order, Align)                                                              \
  VideoPixelFormatInfo{                                                        \
      #Name,                                                                   \
      VideoPixelFormat::Name,                                                  \
      ColorModel::Model,                                                       \
      Planes,                                                                  \
      Hsub,                                                                    \
      Vsub,                                                                    \
      BlockPixels,                                                             \
      BlockBytes,                                                              \
      Alpha,                                                                   \
      ByteOrder::Order,                                                        \
      Align},

constexpr VideoPixelFormatInfo kFormats[]{
    SCORE_VIDEO_PIXEL_FORMATS(SCORE_VPF_INFO_ROW)};

#undef SCORE_VPF_INFO_ROW

static_assert(
    std::size(kFormats) == formatCount(),
    "descriptor array and formatCount() must both come from the table");

} // namespace

const VideoPixelFormatInfo& formatInfo(VideoPixelFormat f) noexcept
{
  // Linear scan over a small constexpr array. The values are sparse (1..100 for
  // ~70 formats) so a direct-indexed table would be mostly holes, and this runs
  // at setup time, not per frame.
  for(const auto& i : kFormats)
    if(i.format == f)
      return i;
  return kUnknown;
}

const char* formatName(VideoPixelFormat f) noexcept
{
  return formatInfo(f).name;
}

const VideoPixelFormatInfo* allFormats(std::size_t& count) noexcept
{
  count = std::size(kFormats);
  return kFormats;
}

VideoPixelFormat chromaSwappedTwin(VideoPixelFormat f) noexcept
{
  switch(f)
  {
    case VideoPixelFormat::YVU420P: return VideoPixelFormat::YUV420P;
    case VideoPixelFormat::YVU422P: return VideoPixelFormat::YUV422P;
    case VideoPixelFormat::YVU410P: return VideoPixelFormat::YUV410P;
    case VideoPixelFormat::NV21:    return VideoPixelFormat::NV12;
    case VideoPixelFormat::NV61:    return VideoPixelFormat::NV16;
    case VideoPixelFormat::NV42:    return VideoPixelFormat::NV24;
    default:                        return VideoPixelFormat::Unknown;
  }
}

std::size_t rowBytes(VideoPixelFormat f, uint32_t width) noexcept
{
  const auto& info = formatInfo(f);
  if(!info.valid() || width == 0)
    return 0;
  // Whole blocks only: a partial block still occupies its full width, which is
  // what makes v210's SMPTE stride (48 pixels per 128 bytes) fall out of the
  // same expression as everything else.
  const std::size_t blocks
      = (std::size_t(width) + info.blockPixels - 1u) / info.blockPixels;
  return blocks * info.blockBytes;
}

std::size_t alignedRowBytes(
    VideoPixelFormat f, uint32_t width, std::size_t alignment) noexcept
{
  return alignUp(rowBytes(f, width), alignment);
}

std::size_t defaultStride(VideoPixelFormat f, uint32_t width) noexcept
{
  return alignedRowBytes(f, width, formatInfo(f).preferredStrideAlignment);
}

std::size_t
bytesPerFrame(VideoPixelFormat f, uint32_t width, uint32_t height) noexcept
{
  const auto& info = formatInfo(f);
  if(!info.valid() || width == 0 || height == 0)
    return 0;

  const std::size_t yBytes = defaultStride(f, width) * height;
  if(!info.isPlanar())
    return yBytes;

  // Chroma planes are scaled by the subsampling factors and padded to the same
  // preferred stride as luma. Semi-planar formats gather both chroma components
  // into one plane of double width; fully planar ones use two separate planes,
  // which comes to the same number of samples.
  const auto cWidth = uint32_t(width / info.horizontalSubsampling);
  const auto cHeight = std::size_t(height / info.verticalSubsampling);
  switch(info.planeCount)
  {
    case 2:
      return yBytes + defaultStride(f, cWidth * 2u) * cHeight;
    case 3:
      return yBytes + 2u * (defaultStride(f, cWidth) * cHeight);
    case 4:
      // Planar with a full-resolution alpha plane.
      return yBytes + 2u * (defaultStride(f, cWidth) * cHeight) + yBytes;
    default:
      return yBytes;
  }
}

} // namespace score::gfx::interop
