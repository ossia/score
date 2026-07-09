#pragma once

/**
 * @file WireEncoderFactory.hpp
 * @brief Vendor-neutral GPU-encoder selection keyed on VideoPixelFormat.
 *
 * Every capture-card addon needs to turn "the on-wire pixel format the card
 * wants" into "the GPU encoder that produces those exact bytes". AJA, DeckLink,
 * Bluefish and Deltacast all carry the same handful of SDI/HDMI wire formats;
 * without this factory each addon re-writes the same 60-line switch over its own
 * vendor enum.
 *
 * The addon's job shrinks to a one-line translation table
 * (`vendorFmt -> VideoPixelFormat`); the actual encoder choice + the byte-layout
 * knowledge lives here, once.
 *
 *   - `makeWireEncoder(VideoPixelFormat)` -> render (fragment) encoder. Default
 *     output path (works on every QRhi backend).
 *   - `makeWireComputeEncoder(VideoPixelFormat)` -> compute encoder for the
 *     GPU-direct (tier-3) path that writes straight into an imported buffer.
 *   - `wireComputeSupports(VideoPixelFormat, width)` -> whether the compute
 *     encoder can handle that (format, width) (v210 needs width % 6 == 0, etc).
 *
 * Both return nullptr for formats with no GPU encoder yet (e.g. P210, Mono,
 * Bayer); callers fall back / report unsupported. The mapping reproduces exactly
 * the encoders the AJA path validated on hardware, so an addon that maps its
 * vendor enum to the matching VideoPixelFormat gets byte-identical output.
 */

#include <Gfx/Graph/encoders/BGRA.hpp>
#include <Gfx/Graph/encoders/BGRACompute.hpp>
#include <Gfx/Graph/encoders/ComputeEncoder.hpp>
#include <Gfx/Graph/encoders/GPUVideoEncoder.hpp>
#include <Gfx/Graph/encoders/NV12.hpp>
#include <Gfx/Graph/encoders/P010.hpp>
#include <Gfx/Graph/encoders/PackedRGB.hpp>
#include <Gfx/Graph/encoders/UYVY.hpp>
#include <Gfx/Graph/encoders/UYVYCompute.hpp>
#include <Gfx/Graph/encoders/V210.hpp>
#include <Gfx/Graph/encoders/V210Compute.hpp>
#include <Gfx/Graph/encoders/YUV422P10.hpp>
#include <Gfx/Graph/encoders/YUVPlanar.hpp>
#include <Gfx/Graph/encoders/YUY2.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <memory>

namespace score::gfx
{

/// Render (fragment-shader) encoder producing `fmt`'s exact on-wire bytes.
/// Returns nullptr when no GPU encoder exists for `fmt`.
inline std::unique_ptr<GPUVideoEncoder>
makeWireEncoder(score::gfx::interop::VideoPixelFormat fmt)
{
  using F = score::gfx::interop::VideoPixelFormat;
  switch(fmt)
  {
    // -- packed 8-bit YUV 4:2:2 --
    case F::UYVY422:
      return std::make_unique<UYVYEncoder>();
    case F::YUYV422:
      return std::make_unique<YUY2Encoder>();

    // -- packed 10-bit YUV 4:2:2 --
    case F::V210:
      return std::make_unique<V210Encoder>();

    // -- packed 8-bit RGB (VideoPixelFormat names are *memory* byte order) --
    case F::BGRA8: // memory [B,G,R,A]
      return std::make_unique<BGRAEncoder>(BGRAEncoder::Swizzle::BGRA);
    case F::RGBA8: // memory [R,G,B,A]
      return std::make_unique<BGRAEncoder>(BGRAEncoder::Swizzle::RGBA);
    case F::ARGB8: // memory [A,R,G,B]
      return std::make_unique<BGRAEncoder>(BGRAEncoder::Swizzle::ARGB);
    case F::ABGR8: // memory [A,B,G,R]
      return std::make_unique<BGRAEncoder>(BGRAEncoder::Swizzle::ABGR);
    case F::RGB24:
      return PackedRGBEncoder::rgb24();
    case F::BGR24:
      return PackedRGBEncoder::bgr24();

    // -- packed 10/12-bit RGB --
    case F::R210:
      return PackedRGBEncoder::r210be(); // DeckLink r210: R-high, big-endian
    case F::RGB10:
      return PackedRGBEncoder::rgb10();   // AJA 10BIT_RGB: B-high, little-endian
    case F::ARGB10:
      return PackedRGBEncoder::argb10();
    case F::DPX10:
      return PackedRGBEncoder::dpx10be();
    case F::DPX10LE:
      return PackedRGBEncoder::dpx10le();
    case F::RGB12P:
      return PackedRGBEncoder::rgb12packed();
    case F::RGB48:
      return PackedRGBEncoder::rgb48();

    // -- planar / semi-planar (capture-card output is rare; mostly capture) --
    case F::YUV422P10:
      return std::make_unique<YUV422P10Encoder>();
    case F::YUV422P:
      return YUVPlanarEncoder::p422_8();
    case F::YUV420P:
      return YUVPlanarEncoder::p420_8();
    case F::YUV420P10:
      return YUVPlanarEncoder::p420_10();
    case F::NV12:
      return std::make_unique<NV12Encoder>();
    case F::P010:
      return std::make_unique<P010Encoder>();

    default:
      return nullptr;
  }
}

/// Compute-shader encoder for the GPU-direct path (writes into an imported
/// buffer). Only the formats with a compute variant are supported.
inline std::unique_ptr<ComputeEncoder>
makeWireComputeEncoder(score::gfx::interop::VideoPixelFormat fmt)
{
  using F = score::gfx::interop::VideoPixelFormat;
  switch(fmt)
  {
    case F::V210:
      return std::make_unique<V210ComputeEncoder>();
    case F::UYVY422:
      return std::make_unique<UYVYComputeEncoder>();
    case F::BGRA8:
      return std::make_unique<BGRAComputeEncoder>();
    default:
      return nullptr;
  }
}

/// Whether the compute encoder can handle (fmt, width): v210 needs width % 6,
/// UYVY needs width % 2, BGRA is unconstrained.
inline bool
wireComputeSupports(score::gfx::interop::VideoPixelFormat fmt, int width)
{
  using F = score::gfx::interop::VideoPixelFormat;
  switch(fmt)
  {
    case F::V210:
      return (width % 6) == 0;
    case F::UYVY422:
      return (width % 2) == 0;
    case F::BGRA8:
      return true;
    default:
      return false;
  }
}

} // namespace score::gfx
