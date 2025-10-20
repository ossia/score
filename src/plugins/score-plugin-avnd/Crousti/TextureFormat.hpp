#pragma once
#include <QtGui/private/qrhi_p.h>
#include <avnd/concepts/gfx.hpp>

#include <string_view>
#include <type_traits>

namespace gpp::qrhi
{
template <typename F>
  requires std::is_enum_v<F>
constexpr QRhiTexture::Format textureFormat(F f) noexcept
{
  if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
    if(f == F::RGBA8)
      return QRhiTexture::RGBA8;
  if constexpr(requires { F::BGRA; } || requires { F::BGRA8; })
    if(f == F::BGRA8)
      return QRhiTexture::BGRA8;
  if constexpr(requires { F::R8; } || requires { F::GRAYSCALE; })
    if(f == F::R8)
      return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if constexpr(requires { F::RG8; })
    if(f == F::RG8)
      return QRhiTexture::RG8;
#endif
  if constexpr(requires { F::R16; })
    if(f == F::R16)
      return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if constexpr(requires { F::RG16; })
    if(f == F::RG16)
      return QRhiTexture::RG16;
#endif
  if constexpr(requires { F::RED_OR_ALPHA8; })
    if(f == F::RED_OR_ALPHA8)
      return QRhiTexture::RED_OR_ALPHA8;
  if constexpr(requires { F::RGBA16F; })
    if(f == F::RGBA16F)
      return QRhiTexture::RGBA16F;
  if constexpr(requires { F::RGBA32F; })
    if(f == F::RGBA32F)
      return QRhiTexture::RGBA32F;
  if constexpr(requires { F::R16F; })
    if(f == F::R16F)
      return QRhiTexture::R16F;
  if constexpr(requires { F::R32F; })
    if(f == F::R32F)
      return QRhiTexture::R32F;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if constexpr(requires { F::RGB10A2; })
    if(f == F::RGB10A2)
      return QRhiTexture::RGB10A2;
#endif
  if constexpr(requires { F::D16; })
    if(f == F::D16)
      return QRhiTexture::D16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if constexpr(requires { F::D24; })
    if(f == F::D24)
      return QRhiTexture::D24;
  if constexpr(requires { F::D24S8; })
    if(f == F::D24S8)
      return QRhiTexture::D24S8;
#endif
  if constexpr(requires { F::D32F; })
    if(f == F::D32F)
      return QRhiTexture::D32F;

  if constexpr(requires { F::BC1; })
    if(f == F::BC1)
      return QRhiTexture::BC1;
  if constexpr(requires { F::BC2; })
    if(f == F::BC2)
      return QRhiTexture::BC2;
  if constexpr(requires { F::BC3; })
    if(f == F::BC3)
      return QRhiTexture::BC3;
  if constexpr(requires { F::BC4; })
    if(f == F::BC4)
      return QRhiTexture::BC4;
  if constexpr(requires { F::BC5; })
    if(f == F::BC5)
      return QRhiTexture::BC5;
  if constexpr(requires { F::BC6H; })
    if(f == F::BC6H)
      return QRhiTexture::BC6H;
  if constexpr(requires { F::BC7; })
    if(f == F::BC7)
      return QRhiTexture::BC7;
  if constexpr(requires { F::ETC2_RGB8; })
    if(f == F::ETC2_RGB8)
      return QRhiTexture::ETC2_RGB8;
  if constexpr(requires { F::ETC2_RGB8A1; })
    if(f == F::ETC2_RGB8A1)
      return QRhiTexture::ETC2_RGB8A1;
  if constexpr(requires { F::ETC2_RGB8A8; })
    if(f == F::ETC2_RGBA8)
      return QRhiTexture::ETC2_RGBA8;
  if constexpr(requires { F::ASTC_4X4; })
    if(f == F::ASTC_4x4)
      return QRhiTexture::ASTC_4x4;
  if constexpr(requires { F::ASTC_5X4; })
    if(f == F::ASTC_5x4)
      return QRhiTexture::ASTC_5x4;
  if constexpr(requires { F::ASTC_5X5; })
    if(f == F::ASTC_5x5)
      return QRhiTexture::ASTC_5x5;
  if constexpr(requires { F::ASTC_6X5; })
    if(f == F::ASTC_6x5)
      return QRhiTexture::ASTC_6x5;
  if constexpr(requires { F::ASTC_6X6; })
    if(f == F::ASTC_6x6)
      return QRhiTexture::ASTC_6x6;
  if constexpr(requires { F::ASTC_8X5; })
    if(f == F::ASTC_8x5)
      return QRhiTexture::ASTC_8x5;
  if constexpr(requires { F::ASTC_8X6; })
    if(f == F::ASTC_8x6)
      return QRhiTexture::ASTC_8x6;
  if constexpr(requires { F::ASTC_8X8; })
    if(f == F::ASTC_8x8)
      return QRhiTexture::ASTC_8x8;
  if constexpr(requires { F::ASTC_10X5; })
    if(f == F::ASTC_10x5)
      return QRhiTexture::ASTC_10x5;
  if constexpr(requires { F::ASTC_10X6; })
    if(f == F::ASTC_10x6)
      return QRhiTexture::ASTC_10x6;
  if constexpr(requires { F::ASTC_10X8; })
    if(f == F::ASTC_10x8)
      return QRhiTexture::ASTC_10x8;
  if constexpr(requires { F::ASTC_10X10; })
    if(f == F::ASTC_10x10)
      return QRhiTexture::ASTC_10x10;
  if constexpr(requires { F::ASTC_12X10; })
    if(f == F::ASTC_12x10)
      return QRhiTexture::ASTC_12x10;
  if constexpr(requires { F::ASTC_12X12; })
    if(f == F::ASTC_12x12)
      return QRhiTexture::ASTC_12x12;
  if constexpr(requires { F::RGB; })
    if(f == F::RGB)
      return QRhiTexture::RGBA8; // we'll have a CPU step to go to rgb

  return QRhiTexture::RGBA8;
}

template <typename F>
constexpr QRhiTexture::Format textureFormat() noexcept
{
  if constexpr(requires { std::string_view{F::format()}; })
  {
    constexpr std::string_view fmt = F::format();

    if(fmt == "rgba" || fmt == "rgba8")
      return QRhiTexture::RGBA8;
    else if(fmt == "bgra" || fmt == "bgra8")
      return QRhiTexture::BGRA8;
    else if(fmt == "r8")
      return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if(fmt == "rg8")
      return QRhiTexture::RG8;
#endif
    else if(fmt == "r16")
      return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if(fmt == "rg16")
      return QRhiTexture::RG16;
#endif
    else if(fmt == "red_or_alpha8")
      return QRhiTexture::RED_OR_ALPHA8;
    else if(fmt == "rgba16f")
      return QRhiTexture::RGBA16F;
    else if(fmt == "rgba32f")
      return QRhiTexture::RGBA32F;
    else if(fmt == "r16f")
      return QRhiTexture::R16F;
    else if(fmt == "r32")
      return QRhiTexture::R32F;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if(fmt == "rgb10a2")
      return QRhiTexture::RGB10A2;
#endif

    else if(fmt == "d16")
      return QRhiTexture::D16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if(fmt == "d24")
      return QRhiTexture::D24;
    else if(fmt == "d24s8")
      return QRhiTexture::D24S8;
#endif
    else if(fmt == "d32f")
      return QRhiTexture::D32F;

    else if(fmt == "bc1")
      return QRhiTexture::BC1;
    else if(fmt == "bc2")
      return QRhiTexture::BC2;
    else if(fmt == "bc3")
      return QRhiTexture::BC3;
    else if(fmt == "bc4")
      return QRhiTexture::BC4;
    else if(fmt == "bc5")
      return QRhiTexture::BC5;
    else if(fmt == "bc6h")
      return QRhiTexture::BC6H;
    else if(fmt == "bc7")
      return QRhiTexture::BC7;
    else if(fmt == "etc2_rgb8")
      return QRhiTexture::ETC2_RGB8;
    else if(fmt == "etc2_rgb8a1")
      return QRhiTexture::ETC2_RGB8A1;
    else if(fmt == "etc2_rgb8a8")
      return QRhiTexture::ETC2_RGBA8;
    else if(fmt == "astc_4x4")
      return QRhiTexture::ASTC_4x4;
    else if(fmt == "astc_5x4")
      return QRhiTexture::ASTC_5x4;
    else if(fmt == "astc_5x5")
      return QRhiTexture::ASTC_5x5;
    else if(fmt == "astc_6x5")
      return QRhiTexture::ASTC_6x5;
    else if(fmt == "astc_6x6")
      return QRhiTexture::ASTC_6x6;
    else if(fmt == "astc_8x5")
      return QRhiTexture::ASTC_8x5;
    else if(fmt == "astc_8x6")
      return QRhiTexture::ASTC_8x6;
    else if(fmt == "astc_8x8")
      return QRhiTexture::ASTC_8x8;
    else if(fmt == "astc_10x5")
      return QRhiTexture::ASTC_10x5;
    else if(fmt == "astc_10x6")
      return QRhiTexture::ASTC_10x6;
    else if(fmt == "astc_10x8")
      return QRhiTexture::ASTC_10x8;
    else if(fmt == "astc_10x10")
      return QRhiTexture::ASTC_10x10;
    else if(fmt == "astc_12x10")
      return QRhiTexture::ASTC_12x10;
    else if(fmt == "astc_12x12")
      return QRhiTexture::ASTC_12x12;
    else if(fmt == "rgb")
      return QRhiTexture::RGBA8;
    else
      return QRhiTexture::RGBA8;
  }
  else if constexpr(std::is_enum_v<typename F::format>)
  {
    if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
      return QRhiTexture::RGBA8;
    else if constexpr(requires { F::BGRA; } || requires { F::BGRA8; })
      return QRhiTexture::BGRA8;
    else if constexpr(requires { F::R8; } || requires { F::GRAYSCALE; })
      return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if constexpr(requires { F::RG8; })
      return QRhiTexture::RG8;
#endif
    else if constexpr(requires { F::R16; })
      return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if constexpr(requires { F::RG16; })
      return QRhiTexture::RG16;
#endif
    else if constexpr(requires { F::RED_OR_ALPHA8; })
      return QRhiTexture::RED_OR_ALPHA8;
    else if constexpr(requires { F::RGBA16F; })
      return QRhiTexture::RGBA16F;
    else if constexpr(requires { F::RGBA32F; })
      return QRhiTexture::RGBA32F;
    else if constexpr(requires { F::R16F; })
      return QRhiTexture::R16F;
    else if constexpr(requires { F::R32F; })
      return QRhiTexture::R32F;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if constexpr(requires { F::RGB10A2; })
      return QRhiTexture::RGB10A2;
#endif
    else if constexpr(requires { F::D16; })
      return QRhiTexture::D16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if constexpr(requires { F::D24; })
      return QRhiTexture::D24;
    else if constexpr(requires { F::D24S8; })
      return QRhiTexture::D24S8;
#endif
    else if constexpr(requires { F::D32F; })
      return QRhiTexture::D32F;

    else if constexpr(requires { F::BC1; })
      return QRhiTexture::BC1;
    else if constexpr(requires { F::BC2; })
      return QRhiTexture::BC2;
    else if constexpr(requires { F::BC3; })
      return QRhiTexture::BC3;
    else if constexpr(requires { F::BC4; })
      return QRhiTexture::BC4;
    else if constexpr(requires { F::BC5; })
      return QRhiTexture::BC5;
    else if constexpr(requires { F::BC6H; })
      return QRhiTexture::BC6H;
    else if constexpr(requires { F::BC7; })
      return QRhiTexture::BC7;
    else if constexpr(requires { F::ETC2_RGB8; })
      return QRhiTexture::ETC2_RGB8;
    else if constexpr(requires { F::ETC2_RGB8A1; })
      return QRhiTexture::ETC2_RGB8A1;
    else if constexpr(requires { F::ETC2_RGB8A8; })
      return QRhiTexture::ETC2_RGBA8;
    else if constexpr(requires { F::ASTC_4X4; })
      return QRhiTexture::ASTC_4x4;
    else if constexpr(requires { F::ASTC_5X4; })
      return QRhiTexture::ASTC_5x4;
    else if constexpr(requires { F::ASTC_5X5; })
      return QRhiTexture::ASTC_5x5;
    else if constexpr(requires { F::ASTC_6X5; })
      return QRhiTexture::ASTC_6x5;
    else if constexpr(requires { F::ASTC_6X6; })
      return QRhiTexture::ASTC_6x6;
    else if constexpr(requires { F::ASTC_8X5; })
      return QRhiTexture::ASTC_8x5;
    else if constexpr(requires { F::ASTC_8X6; })
      return QRhiTexture::ASTC_8x6;
    else if constexpr(requires { F::ASTC_8X8; })
      return QRhiTexture::ASTC_8x8;
    else if constexpr(requires { F::ASTC_10X5; })
      return QRhiTexture::ASTC_10x5;
    else if constexpr(requires { F::ASTC_10X6; })
      return QRhiTexture::ASTC_10x6;
    else if constexpr(requires { F::ASTC_10X8; })
      return QRhiTexture::ASTC_10x8;
    else if constexpr(requires { F::ASTC_10X10; })
      return QRhiTexture::ASTC_10x10;
    else if constexpr(requires { F::ASTC_12X10; })
      return QRhiTexture::ASTC_12x10;
    else if constexpr(requires { F::ASTC_12X12; })
      return QRhiTexture::ASTC_12x12;
    else if constexpr(requires { F::RGB; })
      return QRhiTexture::RGBA8;
    else
      return QRhiTexture::RGBA8;
  }
}

template <avnd::cpu_texture Tex>
constexpr QRhiTexture::Format textureFormat(const Tex& t) noexcept
{
  QRhiTexture::Format fmt{};
  if constexpr(avnd::cpu_dynamic_format_texture<Tex>)
  {
    fmt = gpp::qrhi::textureFormat(t.format);
  }
  else
  {
    constexpr auto c_fmt = gpp::qrhi::textureFormat<Tex>();
    fmt = c_fmt;
  }
  return fmt;
}

}
