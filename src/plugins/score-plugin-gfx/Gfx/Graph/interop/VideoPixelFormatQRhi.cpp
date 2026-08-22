#include <Gfx/Graph/interop/VideoPixelFormatQRhi.hpp>

namespace score::gfx::interop
{

QRhiTexture::Format planeTextureFormat(VideoPixelFormat f, int plane) noexcept
{
  const auto& info = formatInfo(f);
  if(!info.valid() || plane < 0 || plane >= info.planeCount)
    return QRhiTexture::UnknownFormat;

  if(info.isPlanar())
  {
    // One texture per plane. A semi-planar chroma plane carries two components
    // per site, a fully planar one carries a single component.
    const bool chroma = plane > 0;
    const bool twoComponents = chroma && info.planeCount == 2;
    const bool sixteenBit = info.blockBytes == 2;
    if(twoComponents)
      return sixteenBit ? QRhiTexture::RG16 : QRhiTexture::RG8;
    return sixteenBit ? QRhiTexture::R16 : QRhiTexture::R8;
  }

  // Packed. Everything a shader can unpack is sampled as some fixed-width
  // texture; the format below is the sampling format, not a statement about the
  // colour model.
  switch(f)
  {
    case VideoPixelFormat::BGRA8:
    case VideoPixelFormat::BGRX8:
      return QRhiTexture::BGRA8;

    case VideoPixelFormat::RGBA8:
    case VideoPixelFormat::RGBX8:
    case VideoPixelFormat::ARGB8:
    case VideoPixelFormat::ABGR8:
    case VideoPixelFormat::XRGB8:
    case VideoPixelFormat::XBGR8:
    // Packed 4:2:2 and 4:4:4 YUV: four bytes per texel, unpacked in a shader.
    case VideoPixelFormat::UYVY422:
    case VideoPixelFormat::YUYV422:
    case VideoPixelFormat::YVYU422:
    case VideoPixelFormat::VYUY422:
    case VideoPixelFormat::VUYA:
    case VideoPixelFormat::VUYX:
    case VideoPixelFormat::AYUV:
    case VideoPixelFormat::XYUV:
    case VideoPixelFormat::YUVA:
    case VideoPixelFormat::YUVX:
      return QRhiTexture::RGBA8;

    case VideoPixelFormat::X2RGB10:
    case VideoPixelFormat::X2BGR10:
      return QRhiTexture::RGB10A2;

    case VideoPixelFormat::RGBA16:
    case VideoPixelFormat::Y210:
    case VideoPixelFormat::Y216:
    case VideoPixelFormat::V216:
    case VideoPixelFormat::AYUV64:
      return QRhiTexture::RGBA16F;
    case VideoPixelFormat::RGBA16F:
      return QRhiTexture::RGBA16F;
    case VideoPixelFormat::RGBA32F:
      return QRhiTexture::RGBA32F;

    case VideoPixelFormat::Mono8:
    case VideoPixelFormat::BayerBGGR8:
    case VideoPixelFormat::BayerGBRG8:
    case VideoPixelFormat::BayerGRBG8:
    case VideoPixelFormat::BayerRGGB8:
    case VideoPixelFormat::BayerRG8:
      return QRhiTexture::R8;
    case VideoPixelFormat::Mono10:
    case VideoPixelFormat::Mono12:
    case VideoPixelFormat::Mono16:
    case VideoPixelFormat::Mono16BE:
    case VideoPixelFormat::BayerBGGR16:
    case VideoPixelFormat::BayerRGGB16:
    case VideoPixelFormat::BayerRG12:
    case VideoPixelFormat::BayerBGGR10:
    case VideoPixelFormat::BayerGBRG10:
    case VideoPixelFormat::BayerGRBG10:
    case VideoPixelFormat::BayerRGGB10:
      return QRhiTexture::R16;

    default:
      // The wire-only layouts (v210, r210, DPX, 12-bit packed, the sub-byte RGB
      // group) have no texture a shader can sample directly; they are decoded
      // into one of the above first.
      return QRhiTexture::UnknownFormat;
  }
}

uint32_t
planeTextureWidth(VideoPixelFormat f, int plane, uint32_t width) noexcept
{
  const auto& info = formatInfo(f);
  if(!info.valid() || plane < 0 || plane >= info.planeCount || width == 0)
    return 0;
  if(info.isPlanar())
  {
    if(plane == 0 || plane == 3)
      return width;
    return (width + info.horizontalSubsampling - 1) / info.horizontalSubsampling;
  }
  // Packed: as many texels as the block geometry needs. UYVY422 packs two pixels
  // into one RGBA8 texel, so a 1920-pixel row is 960 texels wide.
  const auto texelBytes = 4u;
  const auto bytes = rowBytes(f, width);
  return uint32_t((bytes + texelBytes - 1) / texelBytes);
}

uint32_t
planeTextureHeight(VideoPixelFormat f, int plane, uint32_t height) noexcept
{
  const auto& info = formatInfo(f);
  if(!info.valid() || plane < 0 || plane >= info.planeCount || height == 0)
    return 0;
  if(info.isPlanar() && plane > 0 && plane < 3)
    return (height + info.verticalSubsampling - 1) / info.verticalSubsampling;
  return height;
}

VideoPixelFormat fromTextureFormat(QRhiTexture::Format f) noexcept
{
  // Only the unambiguous RGB formats. A shader-unpacked YUV texture is RGBA8
  // like any other, so the direction genuinely cannot be inverted for those.
  switch(f)
  {
    case QRhiTexture::BGRA8:
      return VideoPixelFormat::BGRA8;
    case QRhiTexture::RGBA8:
      return VideoPixelFormat::RGBA8;
    case QRhiTexture::RGBA16F:
      return VideoPixelFormat::RGBA16F;
    case QRhiTexture::RGBA32F:
      return VideoPixelFormat::RGBA32F;
    case QRhiTexture::RGB10A2:
      return VideoPixelFormat::X2RGB10;
    case QRhiTexture::R8:
      return VideoPixelFormat::Mono8;
    case QRhiTexture::R16:
      return VideoPixelFormat::Mono16;
    default:
      return VideoPixelFormat::Unknown;
  }
}

} // namespace score::gfx::interop
