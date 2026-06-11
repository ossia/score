#pragma once

/**
 * @file VideoPixelFormat.hpp
 * @brief Vendor-neutral pixel format enum + descriptive metadata.
 *
 * Capture/output cards each have their own pixel-format enums:
 * AJA `NTV2FrameBufferFormat`, DeckLink `BMDPixelFormat`, Magewell
 * `MWFOURCC`, Rivermax SDP-derived sample types, Ximea `XI_IMG_FORMAT`.
 * Strategies translate between vendor enums and score's internal
 * representation; without a central enum every strategy reinvents the
 * mapping table.
 *
 * This file provides:
 *
 *   - `VideoPixelFormat` — exhaustive enum of formats score handles
 *     (packed RGB, packed YUV, planar YUV, 10/12-bit variants, etc).
 *   - `VideoPixelFormatInfo` — descriptive struct with bytes-per-pixel
 *     equivalent, plane count, subsampling, default stride alignment,
 *     and a human-readable name.
 *   - Helpers: `formatInfo()`, `defaultStride()`, `formatName()`,
 *     `bytesPerFrame()`.
 *
 * Vendor-specific translation tables (e.g. `bmdFormatTo(...)`,
 * `ntv2FormatTo(...)`) live in each vendor's addon to avoid pulling
 * vendor headers into score-plugin-gfx.
 *
 * Eventually this header may move to a `score-lib-video` library when
 * one exists; for now it lives under `score::gfx::interop` since
 * score-plugin-gfx is the universal dependency.
 */

#include <score_plugin_gfx_export.h>

#include <cstddef>
#include <cstdint>

namespace score::gfx::interop
{

/** Comprehensive pixel format enum covering every flavour score's
 *  vendor strategies need. Values are stable; reorder only with
 *  serialization migration. */
enum class VideoPixelFormat : uint16_t
{
  Unknown = 0,

  // -- Packed 8-bit RGB ------------------------------------------------
  BGRA8 = 1,      /**< Most common; matches QRhi BGRA8 + DeckLink bmdFormat8BitBGRA. */
  RGBA8 = 2,
  ARGB8 = 3,
  ABGR8 = 4,

  // -- Packed 10/12-bit RGB --------------------------------------------
  R210 = 10,      /**< 10-bit RGB, 4 bytes/pixel, big-endian word packing. */
  R12B = 11,      /**< 12-bit RGB big-endian, AJA + DeckLink. */
  R12L = 12,      /**< 12-bit RGB little-endian. */

  // -- Packed 8-bit YUV 4:2:2 -----------------------------------------
  UYVY422 = 20,   /**< Most common SDI; 16-bpp packed UYVY. */
  YUYV422 = 21,
  YVYU422 = 22,
  VYUY422 = 23,

  // -- Packed 10-bit YUV 4:2:2 ----------------------------------------
  V210 = 30,      /**< SMPTE 296M; 6 pixels packed into 16 bytes. */
  V216 = 31,      /**< 16-bit per channel; 4:2:2. */

  // -- Planar 4:2:0 ----------------------------------------------------
  NV12 = 40,      /**< Y plane + interleaved UV plane; HEVC standard. */
  P010 = 41,      /**< NV12 with 10-bit upshifted to 16-bit lanes. */
  YUV420P = 42,   /**< Three-plane 4:2:0; FFmpeg standard. */

  // -- Planar 4:2:2 ----------------------------------------------------
  P210 = 50,      /**< NV12-shape with 4:2:2 sub-sampling at 10-bit. */
  YUV422P = 51,
  YUV422P10 = 52,

  // -- Planar 4:4:4 ----------------------------------------------------
  YUV444P = 60,
  YUV444P10 = 61,
  YUV444P12 = 62,

  // -- High-precision RGB ----------------------------------------------
  RGBA16 = 70,    /**< 16-bit per channel integer RGBA. */
  RGBA16F = 71,   /**< 16-bit half-float per channel. */
  RGBA32F = 72,   /**< 32-bit float per channel. */

  // -- Raw / Bayer (industrial cameras) -------------------------------
  Mono8 = 80,
  Mono10 = 81,
  Mono12 = 82,
  Mono16 = 83,
  BayerRG8 = 84,
  BayerRG12 = 85,
};

/** Sample arrangement description. Three plane counts cover everything
 *  the matrix vendors emit:
 *   - 1: packed (BGRA8, UYVY, V210, R210, Mono*)
 *   - 2: semi-planar (NV12, P010, P210)
 *   - 3: fully planar (YUV420P, YUV422P, etc) */
struct VideoPixelFormatInfo
{
  const char* name{"unknown"};
  uint8_t bitsPerPixel{};       /**< Average; 12 for NV12, 16 for UYVY, 20 for V210 effective. */
  uint8_t planeCount{1};
  uint8_t horizontalSubsampling{1}; /**< Chroma subsampling: 1 for 4:4:4, 2 for 4:2:2, 2 for 4:2:0. */
  uint8_t verticalSubsampling{1};   /**< 1 for 4:2:2, 2 for 4:2:0. */
  bool isYuv{};
  bool isPlanar{};
  uint16_t defaultStrideAlignment{256}; /**< Bytes; vendors typically need 128 or 256. */
};

/** Round `v` up to the next multiple of `a` (`a` must be a power of two). */
constexpr std::size_t alignUp(std::size_t v, std::size_t a) noexcept
{
  return (v + a - 1) & ~(a - 1);
}

/** Get the descriptive info for `f`. Returns a static reference; the
 *  pointer is stable for the duration of the program. */
SCORE_PLUGIN_GFX_EXPORT
const VideoPixelFormatInfo& formatInfo(VideoPixelFormat f) noexcept;

/** Human-readable short name, e.g. "UYVY422", "V210". */
SCORE_PLUGIN_GFX_EXPORT
const char* formatName(VideoPixelFormat f) noexcept;

/** Compute the default row stride in bytes for `f` at the given
 *  `width`. The result is rounded up to `formatInfo(f).defaultStrideAlignment`.
 *  Vendor-specific stride rules (V210's `(width+47)/48*128`) are baked
 *  into this function for the cases that need it. */
SCORE_PLUGIN_GFX_EXPORT
std::size_t defaultStride(VideoPixelFormat f, uint32_t width) noexcept;

/** Total byte size of one frame at `width × height` in format `f`.
 *  Accounts for plane count + subsampling. */
SCORE_PLUGIN_GFX_EXPORT
std::size_t bytesPerFrame(
    VideoPixelFormat f, uint32_t width, uint32_t height) noexcept;

} // namespace score::gfx::interop
