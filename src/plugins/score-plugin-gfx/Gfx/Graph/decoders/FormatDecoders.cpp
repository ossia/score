#include "FormatDecoders.hpp"

namespace score::gfx::interop
{

const char* decoderFragmentForFormat(VideoPixelFormat f) noexcept
{
  switch(f)
  {
    case VideoPixelFormat::UYVY422:
      return uyvyDecoderFrag;
    case VideoPixelFormat::V210:
      return v210DecoderFrag;
    case VideoPixelFormat::NV12:
    case VideoPixelFormat::P010: // same shader; sample range expanded externally
      return nv12DecoderFrag;

    // YUYV422 / VYUY422 / YVYU422 are byte-swapped UYVY variants — a
    // permutation of `t.{r,g,b,a}` in the UYVY shader. Until a consumer
    // needs them, returning null tells the strategy "use CPU swap to
    // UYVY first" which is the path AJA's existing capture takes.
    case VideoPixelFormat::YUYV422:
    case VideoPixelFormat::YVYU422:
    case VideoPixelFormat::VYUY422:
      return nullptr;

    // Pass-through formats — sampler2D returns RGBA directly, no decode
    // shader needed; the vendor strategy uses a trivial blit.
    case VideoPixelFormat::BGRA8:
    case VideoPixelFormat::RGBA8:
    case VideoPixelFormat::ARGB8:
    case VideoPixelFormat::ABGR8:
    case VideoPixelFormat::RGBA16:
    case VideoPixelFormat::RGBA16F:
    case VideoPixelFormat::RGBA32F:
      return nullptr;

    // High-bit-depth packed RGB and planar YUV beyond NV12/P010 —
    // decoders not yet written. Consumers fall back to CPU repack +
    // RGBA8 upload until a vendor strategy demands one of these.
    default:
      return nullptr;
  }
}

} // namespace score::gfx::interop
