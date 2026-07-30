#include <Gfx/Graph/interop/GStreamerPixelFormat.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace score::gfx::interop
{
namespace
{
using Row = std::pair<std::string_view, VideoPixelFormat>;
using V = VideoPixelFormat;

// GStreamer names the memory byte order directly, so these read literally: its
// "RGBA" is R,G,B,A in memory. The V-before-U spellings are distinct layouts, not
// their U-first twins with a flag.
constexpr std::array kRows{
    // packed 8-bit RGB
    Row{"RGBA", V::RGBA8}, Row{"BGRA", V::BGRA8},
    Row{"ARGB", V::ARGB8}, Row{"ABGR", V::ABGR8},
    Row{"RGBx", V::RGBX8}, Row{"BGRx", V::BGRX8},
    Row{"xRGB", V::XRGB8}, Row{"xBGR", V::XBGR8},
    Row{"RGB", V::RGB24},  Row{"BGR", V::BGR24},
    Row{"RGB16", V::RGB565}, Row{"RGB15", V::RGB555},
    // packed 10-bit RGB
    Row{"r210", V::R210},
    // packed YUV
    Row{"YUY2", V::YUYV422}, Row{"YVYU", V::YVYU422},
    Row{"UYVY", V::UYVY422}, Row{"VYUY", V::VYUY422},
    Row{"AYUV", V::AYUV},    Row{"VUYA", V::VUYA},
    Row{"v210", V::V210},    Row{"v216", V::V216},
    Row{"Y210", V::Y210},    Row{"Y410", V::XV30},
    Row{"IYU1", V::UYYVYY411},
    // planar, U before V
    Row{"I420", V::YUV420P}, Row{"Y42B", V::YUV422P},
    Row{"Y444", V::YUV444P}, Row{"Y41B", V::YUV411P},
    Row{"YUV9", V::YUV410P},
    // planar, V before U
    Row{"YV12", V::YVU420P}, Row{"YVU9", V::YVU410P},
    // planar high bit depth
    Row{"I420_10LE", V::YUV420P10}, Row{"I422_10LE", V::YUV422P10},
    Row{"Y444_10LE", V::YUV444P10}, Row{"I422_12LE", V::YUV422P12},
    Row{"Y444_12LE", V::YUV444P12}, Row{"I422_16LE", V::YUV422P16},
    Row{"A444", V::YUVA444P},
    // semi-planar
    Row{"NV12", V::NV12}, Row{"NV21", V::NV21},
    Row{"NV16", V::NV16}, Row{"NV61", V::NV61},
    Row{"NV24", V::NV24},
    Row{"P010_10LE", V::P010}, Row{"P016_LE", V::P216},
    // greyscale
    Row{"GRAY8", V::Mono8},
    Row{"GRAY16_LE", V::Mono16}, Row{"GRAY16_BE", V::Mono16BE},
};
} // namespace

VideoPixelFormat fromGStreamerFormat(std::string_view name) noexcept
{
  const auto it = std::find_if(
      kRows.begin(), kRows.end(), [name](const Row& r) { return r.first == name; });
  return it != kRows.end() ? it->second : V::Unknown;
}

std::string_view toGStreamerFormat(VideoPixelFormat f) noexcept
{
  const auto it = std::find_if(
      kRows.begin(), kRows.end(), [f](const Row& r) { return r.second == f; });
  return it != kRows.end() ? it->first : std::string_view{};
}

} // namespace score::gfx::interop
