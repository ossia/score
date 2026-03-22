#include <Gfx/Graph/decoders/GPUVideoDecoderFactory.hpp>

#include <Gfx/Graph/decoders/DXV.hpp>
#include <Gfx/Graph/decoders/HAP.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/NV16.hpp>
#include <Gfx/Graph/decoders/NV24.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Gfx/Graph/decoders/P016.hpp>
#include <Gfx/Graph/decoders/P210.hpp>
#include <Gfx/Graph/decoders/P410.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
#include <Gfx/Graph/decoders/VUYA.hpp>
#include <Gfx/Graph/decoders/Y210.hpp>
#include <Gfx/Graph/decoders/YUV420.hpp>
#include <Gfx/Graph/decoders/YUV420P10.hpp>
#include <Gfx/Graph/decoders/YUV420P12.hpp>
#include <Gfx/Graph/decoders/YUV422.hpp>
#include <Gfx/Graph/decoders/YUV422P10.hpp>
#include <Gfx/Graph/decoders/YUV422P12.hpp>
#include <Gfx/Graph/decoders/YUV440.hpp>
#include <Gfx/Graph/decoders/YUV444.hpp>
#include <Gfx/Graph/decoders/YUV444P10.hpp>
#include <Gfx/Graph/decoders/YUV444P12.hpp>
#include <Gfx/Graph/decoders/YUVA420.hpp>
#include <Gfx/Graph/decoders/YUVA444.hpp>
#include <Gfx/Graph/decoders/YUYV422.hpp>

extern "C" {
#include <libavutil/pixdesc.h>
}

#include <QString>

#include <string_view>

namespace score::gfx
{

std::unique_ptr<GPUVideoDecoder> createGPUVideoDecoder(
    Video::ImageFormat& format, const std::string& filter)
{
  QString f = QString::fromStdString(filter);

  switch(format.pixel_format)
  {
    // 420P
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      return std::make_unique<YUV420Decoder>(format);
    case AV_PIX_FMT_YUV420P10LE:
      return std::make_unique<YUV420P10Decoder>(format);
    case AV_PIX_FMT_YUV420P12LE:
      return std::make_unique<YUV420P12Decoder>(format);
    case AV_PIX_FMT_NV12:
      return std::make_unique<NV12Decoder>(format, false);
    case AV_PIX_FMT_NV21:
      return std::make_unique<NV12Decoder>(format, true);
    case AV_PIX_FMT_P010LE:
      return std::make_unique<P010Decoder>(format);
    case AV_PIX_FMT_P016LE:
      return std::make_unique<P016Decoder>(format);

    // 420P + alpha
    case AV_PIX_FMT_YUVA420P:
      return std::make_unique<YUVA420Decoder>(format);

    // 444P
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
      return std::make_unique<YUV444Decoder>(format);
    case AV_PIX_FMT_YUV444P10LE:
      return std::make_unique<YUV444P10Decoder>(format);
    case AV_PIX_FMT_YUV444P12LE:
      return std::make_unique<YUV444P12Decoder>(format);
    case AV_PIX_FMT_YUVA444P:
      return std::make_unique<YUVA444Decoder>(format);
    case AV_PIX_FMT_YUVA444P10LE:
      return std::make_unique<YUVA444P10Decoder>(format);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_YUVA444P12LE:
      return std::make_unique<YUVA444P12Decoder>(format);
#endif

    // 440P
    case AV_PIX_FMT_YUV440P:
    case AV_PIX_FMT_YUVJ440P:
      return std::make_unique<YUV440Decoder>(format);

    // 422P
    case AV_PIX_FMT_YUVJ422P:
    case AV_PIX_FMT_YUV422P:
      return std::make_unique<YUV422Decoder>(format);
    case AV_PIX_FMT_YUV422P10LE:
      return std::make_unique<YUV422P10Decoder>(format);
    case AV_PIX_FMT_YUV422P12LE:
      return std::make_unique<YUV422P12Decoder>(format);

    // Semi-planar 422
    case AV_PIX_FMT_NV16:
      return std::make_unique<NV16Decoder>(format);

    // YUYV
    case AV_PIX_FMT_UYVY422:
      return std::make_unique<UYVY422Decoder>(format);
    case AV_PIX_FMT_YUYV422:
      return std::make_unique<YUYV422Decoder>(format);

    // RGB24
    case AV_PIX_FMT_RGB24:
      return std::make_unique<RGB24Decoder>(
          format, "processed.a = 1.0; " + f);
    case AV_PIX_FMT_BGR24:
      return std::make_unique<RGB24Decoder>(
          format, "processed.rgb = tex.bgr; processed.a = 1.0; " + f);

    // RGB48
    case AV_PIX_FMT_RGB48LE:
      return std::make_unique<RGB48Decoder>(
          format, "processed.a = 1.0; " + f);
    case AV_PIX_FMT_BGR48LE:
      return std::make_unique<RGB48Decoder>(
          format, "processed.rgb = tex.bgr; processed.a = 1.0; " + f);

    // RGBA
    case AV_PIX_FMT_RGB0:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, format, "processed.a = 1.0; " + f);
    case AV_PIX_FMT_RGBA:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, format, f);
    case AV_PIX_FMT_BGR0:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, format, "processed.a = 1.0; " + f);
    case AV_PIX_FMT_BGRA:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::BGRA8, 4, format, f);
    case AV_PIX_FMT_ARGB:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, format,
          "processed.rgba = tex.yzwx; " + f);
    case AV_PIX_FMT_ABGR:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA8, 4, format,
          "processed.rgba = tex.abgr; " + f);

    // RGBA 16-bit
    case AV_PIX_FMT_RGBA64LE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA16F, 8, format, f);
    case AV_PIX_FMT_BGRA64LE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA16F, 8, format,
          "processed.rgba = vec4(tex.b, tex.g, tex.r, tex.a); " + f);

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case AV_PIX_FMT_X2RGB10LE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGB10A2, 4, format,
          "processed.rgba = vec4(tex.b, tex.g, tex.r, 1.0); " + f);
#endif

    // Planar RGB
    case AV_PIX_FMT_GBRP:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R8, 1, "gbr", format, f);
    case AV_PIX_FMT_GBRAP:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R8, 1, "gbra", format, f);

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
    case AV_PIX_FMT_GBRP10LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbr", format,
          "processed.rgb *= 64.0; " + f);
    case AV_PIX_FMT_GBRP12LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbr", format,
          "processed.rgb *= 16.0; " + f);
    case AV_PIX_FMT_GBRP16LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbr", format, f);
    case AV_PIX_FMT_GBRAP10LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbra", format,
          "processed *= 64.0; " + f);
    case AV_PIX_FMT_GBRAP12LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbra", format,
          "processed *= 16.0; " + f);
    case AV_PIX_FMT_GBRAP16LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R16, 2, "gbra", format, f);
    case AV_PIX_FMT_GBRPF32LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R32F, 4, "gbr", format, f);
    case AV_PIX_FMT_GBRAPF32LE:
      return std::make_unique<PlanarDecoder>(
          QRhiTexture::R32F, 4, "gbra", format, f);
    case AV_PIX_FMT_NV24:
      return std::make_unique<NV24Decoder>(format, false);
    case AV_PIX_FMT_NV42:
      return std::make_unique<NV24Decoder>(format, true);
    case AV_PIX_FMT_Y210LE:
      return std::make_unique<Y210Decoder>(format);
#endif

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 17, 100)
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case AV_PIX_FMT_X2BGR10LE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGB10A2, 4, format, "processed.a = 1.0; " + f);
#endif
    case AV_PIX_FMT_P210LE:
      return std::make_unique<P210Decoder>(format);
    case AV_PIX_FMT_P410LE:
      return std::make_unique<P410Decoder>(format);
#endif

    // Grey
    case AV_PIX_FMT_GRAY8:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R8, 1, format,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + f);
    case AV_PIX_FMT_GRAY16:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R16, 2, format,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + f);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(60, 8, 100)
    case AV_PIX_FMT_GRAYF16:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R16F, 2, format,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + f);
    case AV_PIX_FMT_RGBAF32LE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA32F, 16, format, f);
    case AV_PIX_FMT_VUYA:
      return std::make_unique<VUYADecoder>(format, false);
    case AV_PIX_FMT_VUYX:
      return std::make_unique<VUYADecoder>(format, true);
    case AV_PIX_FMT_P012LE:
      return std::make_unique<P016Decoder>(format);
    case AV_PIX_FMT_RGBAF16LE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RGBA16F, 8, format, f);
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case AV_PIX_FMT_XV30LE:
      return std::make_unique<XV30Decoder>(format);
#endif
#endif
    case AV_PIX_FMT_GRAYF32:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::R32F, 4, format,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, 1.0);" + f);

    // Grey + Alpha
    case AV_PIX_FMT_YA8:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RG8, 2, format,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, tex.g);" + f);
    case AV_PIX_FMT_YA16LE:
      return std::make_unique<PackedDecoder>(
          QRhiTexture::RG16, 4, format,
          "processed.rgba = vec4(tex.r, tex.r, tex.r, tex.g);" + f);

    default: {
      // Try fourcc-based formats (HAP, DXV)
      std::string_view fourcc{(const char*)&format.pixel_format, 4};

      if(fourcc == "Hap1")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC1, format, f);
      else if(fourcc == "Hap5")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, format, f);
      else if(fourcc == "HapY")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC3, format,
            HAPDefaultDecoder::ycocg_filter + f);
      else if(fourcc == "HapM")
        return std::make_unique<HAPMDecoder>(format, f);
      else if(fourcc == "HapA")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC4, format, f);
      else if(fourcc == "Hap7")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC7, format, f);
      else if(fourcc == "HapH")
        return std::make_unique<HAPDefaultDecoder>(
            QRhiTexture::BC6H, format, f);
      // DXV
      else if(fourcc == "Dxv1")
        return std::make_unique<DXVDecoder>(
            QRhiTexture::BC1, format, f);
      else if(fourcc == "Dxv5")
        return std::make_unique<DXVDecoder>(
            QRhiTexture::BC3, format, f);
      else if(fourcc == "DxvY")
        return std::make_unique<DXVYCoCgDecoder>(false, format, f);
      else if(fourcc == "DxvA")
        return std::make_unique<DXVYCoCgDecoder>(true, format, f);

      return nullptr;
    }
  }
}

} // namespace score::gfx
