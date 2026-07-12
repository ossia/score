#pragma once

/**
 * @file PackedBitfield.hpp
 * @brief Decoder for RGB packed into bit fields narrower than a byte.
 *
 * Covers the 3-3-2, 5-6-5, 5-5-5 and 4-4-4-4 families that V4L2 and older
 * capture hardware still emit. One parameterised decoder rather than ~25
 * near-identical shaders: each format is a table of (offset, width) per
 * channel plus a container size, which is the only thing that actually
 * differs between them.
 *
 * The container is sampled a byte at a time (R8 / RG8) rather than as R16,
 * because R16 would read the pair in the host's byte order and these formats
 * specify their own. Reassembling from bytes makes the endianness explicit
 * and lets the big-endian variants share the code.
 *
 * Sampling must be NEAREST: PackedDecoder filters Linear by default, which
 * interpolates each byte against its neighbours before this filter can
 * recombine them -- measured on Y16-BE, that alone put the result 9 dB from
 * its little-endian twin. The decoder is therefore built with nearest=true.
 */

#include <Gfx/Graph/decoders/GPUVideoDecoder.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>

#include <QString>

namespace score::gfx
{

/// One channel's placement inside the packed container.
struct BitField
{
  int offset{}; ///< LSB position within the reassembled word.
  int width{};  ///< Bit count; 0 means "channel absent, use 1.0".
};

struct PackedBitfieldLayout
{
  int containerBytes{2}; ///< 1 or 2.
  bool bigEndian{false}; ///< Byte order of the container, not the host's.
  BitField r, g, b, a;
};

/// Builds the PackedDecoder filter that unpacks @p l.
///
/// The word is rebuilt from the sampled bytes, then each field is shifted,
/// masked and normalised by its own maximum, so a 5-bit channel spans the
/// full 0..1 range rather than topping out at 31/255.
inline QString packedBitfieldFilter(const PackedBitfieldLayout& l)
{
  QString word;
  if(l.containerBytes == 1)
  {
    word = "uint(tex.r * 255.0 + 0.5)";
  }
  else
  {
    // tex.r is the first byte in memory, tex.g the second.
    word = l.bigEndian
               ? "(uint(tex.r * 255.0 + 0.5) << 8) | uint(tex.g * 255.0 + 0.5)"
               : "(uint(tex.g * 255.0 + 0.5) << 8) | uint(tex.r * 255.0 + 0.5)";
  }

  auto chan = [](const BitField& f) -> QString {
    if(f.width <= 0)
      return "1.0";
    const unsigned mask = (1u << f.width) - 1u;
    return QString("float((w >> %1) & %2u) / %3.0")
        .arg(f.offset)
        .arg(mask)
        .arg(mask);
  };

  return QString("uint w = %1;"
                 "processed.rgba = vec4(%2, %3, %4, %5);")
      .arg(word)
      .arg(chan(l.r))
      .arg(chan(l.g))
      .arg(chan(l.b))
      .arg(chan(l.a));
}

/// Convenience: the decoder for a packed-bitfield format.
inline std::unique_ptr<GPUVideoDecoder>
makePackedBitfieldDecoder(const PackedBitfieldLayout& l, Video::ImageFormat& d)
{
  return std::make_unique<PackedDecoder>(
      l.containerBytes == 1 ? QRhiTexture::R8 : QRhiTexture::RG8,
      l.containerBytes, d, packedBitfieldFilter(l), /*invertY=*/false,
      /*nearest=*/true);
}

} // namespace score::gfx
