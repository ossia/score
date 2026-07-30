#pragma once

/**
 * @file VideoPixelFormat.hpp
 * @brief Vendor-neutral pixel format vocabulary + descriptive metadata.
 *
 * Capture/output cards each have their own pixel-format enums:
 * AJA `NTV2FrameBufferFormat`, DeckLink `BMDPixelFormat`, Magewell
 * `MWFOURCC`, V4L2 fourccs, DRM fourccs, PipeWire SPA formats, GStreamer
 * format strings, Ximea `XI_IMG_FORMAT`. Strategies translate between those
 * and score's internal representation; without a central vocabulary every
 * strategy reinvents the mapping table, and they drift apart.
 *
 * `AVPixelFormat` cannot serve as that vocabulary on its own: FFmpeg models
 * several formats the SDI cards put on the wire as *codecs* rather than pixel
 * formats (v210, v216, r210, DPX 10/12-bit, 12-bit packed RGB, A2-ARGB10), so
 * they have no `AV_PIX_FMT_*` at all. `VideoPixelFormatAV.hpp` bridges the
 * representable subset in both directions.
 *
 * This vocabulary describes **sample layout only**. Colorimetry -- range,
 * primaries, transfer characteristic, chroma siting, HDR metadata -- is a
 * separate orthogonal axis, owned by `Video::ImageFormat`. Do not add colour
 * fields here: a format says how bytes are arranged, not how to interpret the
 * values in them.
 *
 * Everything is generated from one declarative table,
 * `SCORE_VIDEO_PIXEL_FORMATS`: the enum, the descriptor array, the name
 * lookup, and the list the unit test sweeps. A format therefore cannot be
 * declared without being described -- which is how 28 formats previously ended
 * up declared with a zero-byte descriptor, so that `bytesPerFrame` silently
 * returned 0 for them.
 *
 * Vendor-specific translation tables (e.g. `bmdFormatTo(...)`,
 * `ntv2FormatTo(...)`) live in each vendor's addon to avoid pulling vendor
 * headers into score-plugin-gfx.
 */

#include <score_plugin_gfx_export.h>

#include <cstddef>
#include <cstdint>

namespace score::gfx::interop
{

/** Which colour model the samples carry. A boolean "is YUV" cannot express
 *  greyscale or Bayer, both of which the industrial-camera paths produce. */
enum class ColorModel : uint8_t
{
  Unknown = 0,
  RGB,
  YUV,
  Grey,  /**< Single achromatic channel (Mono*). */
  Bayer, /**< Undemosaiced colour-filter-array data (BayerRG*). */
};

/** Byte order of multi-byte samples. `NA` means the format has no multi-byte
 *  sample whose order could differ -- 8-bit packed layouts, and byte-addressed
 *  ones like RGB24 where component order is part of the format identity. */
enum class ByteOrder : uint8_t
{
  NA = 0,
  Little,
  Big,
};

// ---------------------------------------------------------------------------
// The single declarative table. One row per format; nothing else in this
// vocabulary is maintained by hand.
//
//   X(Name, Value, Model, Planes, Hsub, Vsub, BlockPixels, BlockBytes,
//     Alpha, Order, Align)
//
// `Value` is the serialized wire value and is FROZEN: rows may be reordered
// freely, but a value must never change without a serialization migration.
//
// BlockPixels/BlockBytes describe the **primary plane**: a horizontal run of
// BlockPixels pixels occupies exactly BlockBytes bytes. Packed formats put a
// whole pixel group there (BGRA8 = 1px/4B, UYVY = 2px/4B, v210 = 48px/128B per
// SMPTE ST 2110-20); planar formats put one luma sample there (NV12 = 1px/1B,
// P010 = 1px/2B).
//
// That one mechanism replaces an averaged bits-per-pixel, which could not
// express v210 at all (128 bytes / 48 pixels = 21.33 bits, truncated into an
// integer field and then special-cased in the stride function) and which
// over-computed planar strides (NV12 by 1.5x, P010 by 3x). A new packed layout
// now needs a table row, not another branch.
//
// `Align` is a *preferred* stride alignment, used only by callers that have no
// constraint of their own. It is not a property of the pixel format: a device
// or allocator with a real requirement (D3D12's 256-byte row pitch, a DeckLink
// rowBytes, a V4L2 `bytesperline`) must pass its own to `alignedRowBytes`.
// ---------------------------------------------------------------------------
#define SCORE_VIDEO_PIXEL_FORMATS(X)                                           \
  /* -- Packed 8-bit RGB -------------------------------------------------- */ \
  /* BGRA8 matches QRhi BGRA8 + DeckLink bmdFormat8BitBGRA. The X-variants  */ \
  /* are the same bytes with the 4th/1st channel undefined: forced opaque.  */ \
  X(BGRA8, 1, RGB, 1, 1, 1, 1, 4, true, NA, 256)                               \
  X(RGBA8, 2, RGB, 1, 1, 1, 1, 4, true, NA, 256)                               \
  X(ARGB8, 3, RGB, 1, 1, 1, 1, 4, true, NA, 256)                               \
  X(ABGR8, 4, RGB, 1, 1, 1, 1, 4, true, NA, 256)                               \
  X(RGB24, 5, RGB, 1, 1, 1, 1, 3, false, NA, 64)                               \
  X(BGR24, 6, RGB, 1, 1, 1, 1, 3, false, NA, 64)                               \
  X(BGRX8, 7, RGB, 1, 1, 1, 1, 4, false, NA, 256)                              \
  X(RGBX8, 8, RGB, 1, 1, 1, 1, 4, false, NA, 256)                              \
  X(XRGB8, 9, RGB, 1, 1, 1, 1, 4, false, NA, 256)                              \
  X(XBGR8, 19, RGB, 1, 1, 1, 1, 4, false, NA, 256)                             \
  /* -- Packed 10/12-bit RGB ----------------------------------------------- */ \
  /* R210 is DeckLink r210 / AV_CODEC_ID_R210: (R<<20)|(G<<10)|B, big-endian. */ \
  /* RGB10 is AJA NTV2_FBF_10BIT_RGB: (B<<20)|(G<<10)|R, little-endian.       */ \
  /* R12B/R12L pack 8 pixels into 36 bytes (12 bits per component).           */ \
  X(R210, 10, RGB, 1, 1, 1, 1, 4, false, Big, 256)                             \
  X(R12B, 11, RGB, 1, 1, 1, 8, 36, false, Big, 256)                            \
  X(R12L, 12, RGB, 1, 1, 1, 8, 36, false, Little, 256)                         \
  X(ARGB10, 13, RGB, 1, 1, 1, 1, 5, true, Little, 256)                         \
  X(DPX10, 14, RGB, 1, 1, 1, 1, 4, false, Big, 256)                            \
  X(DPX10LE, 15, RGB, 1, 1, 1, 1, 4, false, Little, 256)                       \
  X(RGB12P, 16, RGB, 1, 1, 1, 2, 9, false, NA, 256)                            \
  X(RGB48, 17, RGB, 1, 1, 1, 1, 6, false, Little, 256)                         \
  X(RGB10, 18, RGB, 1, 1, 1, 1, 4, false, Little, 256)                         \
  /* -- Packed sub-byte RGB / YUV (legacy, embedded, V4L2 cameras) --------- */ \
  X(RGB332, 90, RGB, 1, 1, 1, 1, 1, false, NA, 64)                             \
  X(RGB565, 91, RGB, 1, 1, 1, 1, 2, false, Little, 64)                         \
  X(RGB565BE, 92, RGB, 1, 1, 1, 1, 2, false, Big, 64)                          \
  X(RGB555, 93, RGB, 1, 1, 1, 1, 2, false, Little, 64)                         \
  X(RGB555BE, 94, RGB, 1, 1, 1, 1, 2, false, Big, 64)                          \
  X(ARGB1555, 95, RGB, 1, 1, 1, 1, 2, true, Little, 64)                        \
  X(RGB444, 96, RGB, 1, 1, 1, 1, 2, false, Little, 64)                         \
  X(ARGB4444, 97, RGB, 1, 1, 1, 1, 2, true, Little, 64)                        \
  X(AYUV4444, 98, YUV, 1, 1, 1, 1, 2, true, Little, 64)                        \
  X(AYUV1555, 99, YUV, 1, 1, 1, 1, 2, true, Little, 64)                        \
  X(YUV565, 100, YUV, 1, 1, 1, 1, 2, false, Little, 64)                        \
  /* -- Packed 8-bit YUV 4:2:2 (two pixels share one chroma pair) ---------- */ \
  X(UYVY422, 20, YUV, 1, 2, 1, 2, 4, false, NA, 256)                           \
  X(YUYV422, 21, YUV, 1, 2, 1, 2, 4, false, NA, 256)                           \
  X(YVYU422, 22, YUV, 1, 2, 1, 2, 4, false, NA, 256)                           \
  X(VYUY422, 23, YUV, 1, 2, 1, 2, 4, false, NA, 256)                           \
  /* -- Packed 10/16-bit YUV 4:2:2 ----------------------------------------- */ \
  /* v210 packs 48 pixels into 128 bytes; that block IS the SMPTE stride     */ \
  /* rule, so it needs no special case here.                                 */ \
  X(V210, 30, YUV, 1, 2, 1, 48, 128, false, Little, 128)                       \
  X(V216, 31, YUV, 1, 2, 1, 2, 8, false, Little, 256)                          \
  /* Y210/Y216 carry YUYV component order in 16-bit lanes (Y210 uses the high  */ \
  /* 10 bits); V216 differs from Y216 only in that order.                      */ \
  X(Y210, 106, YUV, 1, 2, 1, 2, 8, false, Little, 256)                         \
  X(Y216, 107, YUV, 1, 2, 1, 2, 8, false, Little, 256)                         \
  /* -- Planar / semi-planar 4:2:0 ----------------------------------------- */ \
  X(NV12, 40, YUV, 2, 2, 2, 1, 1, false, NA, 256)                              \
  X(P010, 41, YUV, 2, 2, 2, 1, 2, false, Little, 256)                          \
  X(YUV420P, 42, YUV, 3, 2, 2, 1, 1, false, NA, 256)                           \
  X(YUV420P10, 43, YUV, 3, 2, 2, 1, 2, false, Little, 256)                     \
  X(NV21, 44, YUV, 2, 2, 2, 1, 1, false, NA, 256)                              \
  X(YVU420P, 45, YUV, 3, 2, 2, 1, 1, false, NA, 256)                           \
  /* -- Planar / semi-planar 4:2:2 ----------------------------------------- */ \
  X(P210, 50, YUV, 2, 2, 1, 1, 2, false, Little, 256)                          \
  X(YUV422P, 51, YUV, 3, 2, 1, 1, 1, false, NA, 256)                           \
  X(YUV422P10, 52, YUV, 3, 2, 1, 1, 2, false, Little, 256)                     \
  X(NV16, 53, YUV, 2, 2, 1, 1, 1, false, NA, 256)                              \
  X(NV61, 54, YUV, 2, 2, 1, 1, 1, false, NA, 256)                              \
  X(YUV422P12, 55, YUV, 3, 2, 1, 1, 2, false, Little, 256)                     \
  X(YUV422P16, 110, YUV, 3, 2, 1, 1, 2, false, Little, 256)                    \
  X(YVU422P, 104, YUV, 3, 2, 1, 1, 1, false, NA, 256)                          \
  X(P216, 108, YUV, 2, 2, 1, 1, 2, false, Little, 256)                         \
  /* -- Planar 4:1:1 and 4:1:0 (webcams, legacy capture) ------------------- */ \
  X(YUV411P, 101, YUV, 3, 4, 1, 1, 1, false, NA, 256)                          \
  X(YUV410P, 102, YUV, 3, 4, 4, 1, 1, false, NA, 256)                          \
  X(YVU410P, 103, YUV, 3, 4, 4, 1, 1, false, NA, 256)                          \
  X(UYYVYY411, 105, YUV, 1, 4, 1, 4, 6, false, NA, 256)                        \
  /* -- Planar / semi-planar / packed 4:4:4 -------------------------------- */ \
  X(YUV444P, 60, YUV, 3, 1, 1, 1, 1, false, NA, 256)                           \
  X(YUV444P10, 61, YUV, 3, 1, 1, 1, 2, false, Little, 256)                     \
  X(YUV444P12, 62, YUV, 3, 1, 1, 1, 2, false, Little, 256)                     \
  X(NV24, 63, YUV, 2, 1, 1, 1, 1, false, NA, 256)                              \
  X(NV42, 64, YUV, 2, 1, 1, 1, 1, false, NA, 256)                              \
  X(VUYA, 65, YUV, 1, 1, 1, 1, 4, true, NA, 256)                               \
  X(VUYX, 66, YUV, 1, 1, 1, 1, 4, false, NA, 256)                              \
  X(AYUV, 67, YUV, 1, 1, 1, 1, 4, true, NA, 256)                               \
  X(XYUV, 68, YUV, 1, 1, 1, 1, 4, false, NA, 256)                              \
  X(YUVA, 69, YUV, 1, 1, 1, 1, 4, true, NA, 256)                               \
  X(YUVX, 73, YUV, 1, 1, 1, 1, 4, false, NA, 256)                              \
  X(YUVA444P, 111, YUV, 4, 1, 1, 1, 1, true, NA, 256)                          \
  X(P416, 109, YUV, 2, 1, 1, 1, 2, false, Little, 256)                         \
  /* XV30 packs 2 padding bits + three 10-bit components into 32 bits; the pad */ \
  /* is not alpha. AYUV64 is the same geometry at 16 bits with real alpha.     */ \
  X(XV30, 112, YUV, 1, 1, 1, 1, 4, false, Little, 256)                         \
  X(AYUV64, 113, YUV, 1, 1, 1, 1, 8, true, Little, 256)                        \
  /* -- High-precision RGB ------------------------------------------------- */ \
  X(RGBA16, 70, RGB, 1, 1, 1, 1, 8, true, Little, 256)                         \
  X(RGBA16F, 71, RGB, 1, 1, 1, 1, 8, true, Little, 256)                        \
  X(RGBA32F, 72, RGB, 1, 1, 1, 1, 16, true, Little, 256)                       \
  /* -- Greyscale / Bayer (industrial cameras) ----------------------------- */ \
  X(Mono8, 80, Grey, 1, 1, 1, 1, 1, false, NA, 64)                             \
  X(Mono10, 81, Grey, 1, 1, 1, 1, 2, false, Little, 64)                        \
  X(Mono12, 82, Grey, 1, 1, 1, 1, 2, false, Little, 64)                        \
  X(Mono16, 83, Grey, 1, 1, 1, 1, 2, false, Little, 64)                        \
  X(BayerRG8, 84, Bayer, 1, 1, 1, 1, 1, false, NA, 64)                         \
  X(BayerRG12, 85, Bayer, 1, 1, 1, 1, 2, false, Little, 64)                    \
  X(Mono16BE, 86, Grey, 1, 1, 1, 1, 2, false, Big, 64)

/** Comprehensive pixel format enum, generated from the table above.
 *  Values are stable; reorder only with serialization migration. */
enum class VideoPixelFormat : uint16_t
{
  Unknown = 0,
#define SCORE_VPF_ENUM_ROW(Name, Value, ...) Name = Value,
  SCORE_VIDEO_PIXEL_FORMATS(SCORE_VPF_ENUM_ROW)
#undef SCORE_VPF_ENUM_ROW
};

/** Number of described formats, excluding `Unknown`. */
constexpr std::size_t formatCount() noexcept
{
#define SCORE_VPF_COUNT_ROW(...) +1
  return std::size_t(0 SCORE_VIDEO_PIXEL_FORMATS(SCORE_VPF_COUNT_ROW));
#undef SCORE_VPF_COUNT_ROW
}

/** Sample arrangement description.
 *
 * Plane counts cover everything the matrix vendors emit:
 *   - 1: packed (BGRA8, UYVY, v210, R210, Mono*)
 *   - 2: semi-planar, chroma interleaved into one plane (NV12, P010, P210)
 *   - 3: fully planar (YUV420P, YUV422P, ...)
 *   - 4: fully planar plus an alpha plane (none declared yet; sized correctly
 *        by `bytesPerFrame` if one is added)
 */
struct VideoPixelFormatInfo
{
  const char* name{"unknown"};
  VideoPixelFormat format{VideoPixelFormat::Unknown};
  ColorModel colorModel{ColorModel::Unknown};
  uint8_t planeCount{1};
  uint8_t horizontalSubsampling{1}; /**< 1 for 4:4:4, 2 for 4:2:2 and 4:2:0. */
  uint8_t verticalSubsampling{1};   /**< 1 for 4:2:2, 2 for 4:2:0. */
  /** Primary-plane block geometry: `blockPixels` pixels occupy `blockBytes`
   *  bytes. See the table header for why this replaces bits-per-pixel. */
  uint16_t blockPixels{1};
  uint16_t blockBytes{};
  bool hasAlpha{};
  ByteOrder byteOrder{ByteOrder::NA};
  /** Preferred stride alignment for callers with no constraint of their own.
   *  Devices with a real requirement must pass theirs to `alignedRowBytes`. */
  uint16_t preferredStrideAlignment{256};

  /** True when chroma lives in its own plane(s) rather than interleaved with
   *  luma inside one packed buffer. */
  constexpr bool isPlanar() const noexcept { return planeCount > 1; }
  constexpr bool isYuv() const noexcept { return colorModel == ColorModel::YUV; }
  constexpr bool isRgb() const noexcept { return colorModel == ColorModel::RGB; }
  /** True when the format carries no chroma at all. */
  constexpr bool isAchromatic() const noexcept
  {
    return colorModel == ColorModel::Grey || colorModel == ColorModel::Bayer;
  }
  /** False only for the `Unknown` sentinel. */
  constexpr bool valid() const noexcept { return blockBytes != 0; }
};

/** Round `v` up to a multiple of `a`. `a <= 1` means no alignment; `a` need not
 *  be a power of two (V210's 128 happens to be, a V4L2 `bytesperline` may not). */
constexpr std::size_t alignUp(std::size_t v, std::size_t a) noexcept
{
  return a <= 1 ? v : ((v + a - 1) / a) * a;
}

/** Descriptive info for `f`. Returns a reference to storage with static
 *  lifetime; the `Unknown` sentinel is returned for unrecognised values. */
SCORE_PLUGIN_GFX_EXPORT
const VideoPixelFormatInfo& formatInfo(VideoPixelFormat f) noexcept;

/** Human-readable short name, e.g. "UYVY422", "V210". */
SCORE_PLUGIN_GFX_EXPORT
const char* formatName(VideoPixelFormat f) noexcept;

/** Every described format, in table order. Lets callers -- and the unit test --
 *  enumerate the vocabulary without maintaining a second copy of the list. */
SCORE_PLUGIN_GFX_EXPORT
const VideoPixelFormatInfo* allFormats(std::size_t& count) noexcept;

/** Tight primary-plane row size in bytes, with no padding at all. */
SCORE_PLUGIN_GFX_EXPORT
std::size_t rowBytes(VideoPixelFormat f, uint32_t width) noexcept;

/** `rowBytes` rounded up to `alignment`. This is the form device paths should
 *  use, passing the alignment their allocator actually requires. */
SCORE_PLUGIN_GFX_EXPORT
std::size_t
alignedRowBytes(VideoPixelFormat f, uint32_t width, std::size_t alignment) noexcept;

/** `alignedRowBytes` using the format's `preferredStrideAlignment`, for callers
 *  that have no constraint of their own. */
SCORE_PLUGIN_GFX_EXPORT
std::size_t defaultStride(VideoPixelFormat f, uint32_t width) noexcept;

/** Total byte size of one frame at `width × height`, summing every plane at the
 *  format's preferred stride. */
SCORE_PLUGIN_GFX_EXPORT
std::size_t
bytesPerFrame(VideoPixelFormat f, uint32_t width, uint32_t height) noexcept;

} // namespace score::gfx::interop
