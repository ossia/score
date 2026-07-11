#pragma once

/**
 * @file WireDecoderFactory.hpp
 * @brief Vendor-neutral GPU-decoder selection keyed on VideoPixelFormat.
 *
 * The capture-side symmetric counterpart to encoders/WireEncoderFactory.hpp:
 * turns "the on-wire pixel format a card delivers" into the GPUVideoDecoder that
 * unpacks those bytes into RGBA at sample time. Every capture-card addon
 * (AJA, and future DeckLink/Bluefish/Magewell/Deltacast) used to hand-roll the
 * same switch in its DMACaptureBackend::makeDecoder; this centralises it so an
 * addon only maps its vendor enum -> VideoPixelFormat.
 *
 * Returns nullptr for formats with no capture decoder yet. The decoder
 * allocates an input texture sized to the wire byte layout (the strategy DMAs
 * into it); `meta` carries the VPID/InfoFrame-derived colour metadata.
 */

#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Gfx/Graph/decoders/R210.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/decoders/V210.hpp>
#include <Gfx/Graph/decoders/YUV422P10.hpp>
#include <Gfx/Graph/decoders/YUYV422.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <Video/VideoInterface.hpp>

#include <memory>

namespace score::gfx
{

inline std::unique_ptr<GPUVideoDecoder>
makeWireDecoder(score::gfx::interop::VideoPixelFormat fmt, Video::ImageFormat& d)
{
  using F = score::gfx::interop::VideoPixelFormat;
  switch(fmt)
  {
    // -- packed 8-bit YUV 4:2:2 --
    case F::UYVY422:
      return std::make_unique<UYVY422Decoder>(d);
    case F::YUYV422:
      return std::make_unique<YUYV422Decoder>(d);

    // -- packed 10-bit YUV 4:2:2 --
    case F::V210:
      return std::make_unique<V210Decoder>(d);

    // -- packed 8-bit RGB (the input texture stores these byte orders) --
    case F::BGRA8: // memory [B,G,R,A]
      return std::make_unique<PackedDecoder>(QRhiTexture::BGRA8, 4, d);
    case F::RGBA8: // memory [R,G,B,A]
      return std::make_unique<PackedDecoder>(QRhiTexture::RGBA8, 4, d);

    // -- packed 10-bit RGB (r210, DeckLink SDI 4:4:4 wire) --
    case F::R210:
      return std::make_unique<R210Decoder>(d);

    // -- packed RGB 24/48-bit --
    case F::RGB24:
    case F::BGR24:
      return std::make_unique<RGB24Decoder>(d);
    case F::RGB48:
      return std::make_unique<RGB48Decoder>(d);

    // -- planar / semi-planar (capture cards that convert on-chip, e.g. Magewell) --
    case F::NV12:
      return std::make_unique<NV12Decoder>(d, /*inverted=*/false);
    case F::P010:
      return std::make_unique<P010Decoder>(d);
    case F::YUV422P10:
      return std::make_unique<YUV422P10Decoder>(d);

    default:
      return nullptr;
  }
}

} // namespace score::gfx
