// score::gfx::createGPUVideoDecoder(): which GPU decoder each AVPixelFormat is
// handed to, and how the generic Packed/Planar decoders are parameterised.
//
// The factory takes only a Video::ImageFormat, so the entire pixel-format ->
// decoder-class switch is reachable with no QRhi, no decoder and no frame. Only
// init() (which builds the shaders and textures) needs a render list, and that
// is where the existing GPU tests take over.
//
// Where the target is one of the two generic decoders, asserting the class
// alone would prove nothing -- a dozen formats share it. Those rows assert the
// texture format, the bytes-per-pixel and the swizzle filter the factory picked,
// which is the whole content of the decision.

#include <Gfx/Graph/decoders/GPUVideoDecoderFactory.hpp>
#include <Gfx/Graph/decoders/NV12.hpp>
#include <Gfx/Graph/decoders/NV16.hpp>
#include <Gfx/Graph/decoders/NV24.hpp>
#include <Gfx/Graph/decoders/P010.hpp>
#include <Gfx/Graph/decoders/P016.hpp>
#include <Gfx/Graph/decoders/P210.hpp>
#include <Gfx/Graph/decoders/P410.hpp>
#include <Gfx/Graph/decoders/RGBA.hpp>
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

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <typeinfo>

using namespace score::gfx;

namespace
{
Video::ImageFormat imageFormat(AVPixelFormat fmt)
{
  Video::ImageFormat d;
  d.width = 1920;
  d.height = 1080;
  d.pixel_format = fmt;
  return d;
}

std::unique_ptr<GPUVideoDecoder> make(AVPixelFormat fmt, std::string filter = {})
{
  auto d = imageFormat(fmt);
  return createGPUVideoDecoder(d, filter);
}

// A fourcc-tagged "pixel format": the default branch reinterprets the enum's
// four bytes as a codec tag.
std::unique_ptr<GPUVideoDecoder> makeFourcc(const char (&tag)[5], std::string f = {})
{
  int raw{};
  std::memcpy(&raw, tag, 4);
  auto d = imageFormat(AVPixelFormat(raw));
  return createGPUVideoDecoder(d, f);
}

template <typename T>
T* as(const std::unique_ptr<GPUVideoDecoder>& d)
{
  return dynamic_cast<T*>(d.get());
}
}

TEST_CASE("Planar YUV decoder selection", "[gfx][video][decoderfactory]")
{
  CHECK(as<YUV420Decoder>(make(AV_PIX_FMT_YUV420P)));
  CHECK(as<YUV420Decoder>(make(AV_PIX_FMT_YUVJ420P)));
  CHECK(as<YUV420P10Decoder>(make(AV_PIX_FMT_YUV420P10LE)));
  CHECK(as<YUV420P12Decoder>(make(AV_PIX_FMT_YUV420P12LE)));
  CHECK(as<YUVA420Decoder>(make(AV_PIX_FMT_YUVA420P)));

  CHECK(as<YUV422Decoder>(make(AV_PIX_FMT_YUV422P)));
  CHECK(as<YUV422Decoder>(make(AV_PIX_FMT_YUVJ422P)));
  CHECK(as<YUV422P10Decoder>(make(AV_PIX_FMT_YUV422P10LE)));
  CHECK(as<YUV422P12Decoder>(make(AV_PIX_FMT_YUV422P12LE)));

  CHECK(as<YUV444Decoder>(make(AV_PIX_FMT_YUV444P)));
  CHECK(as<YUV444Decoder>(make(AV_PIX_FMT_YUVJ444P)));
  CHECK(as<YUV444P10Decoder>(make(AV_PIX_FMT_YUV444P10LE)));
  CHECK(as<YUV444P12Decoder>(make(AV_PIX_FMT_YUV444P12LE)));
  CHECK(as<YUVA444Decoder>(make(AV_PIX_FMT_YUVA444P)));
  CHECK(as<YUVA444P10Decoder>(make(AV_PIX_FMT_YUVA444P10LE)));

  CHECK(as<YUV440Decoder>(make(AV_PIX_FMT_YUV440P)));
  CHECK(as<YUV440Decoder>(make(AV_PIX_FMT_YUVJ440P)));

  SECTION("the deprecated J variants are not routed anywhere else")
  {
    // AVCOL_RANGE is carried separately; the J formats differ only in range and
    // must reach the same decoder as their non-J twin.
    CHECK(typeid(*make(AV_PIX_FMT_YUVJ420P)) == typeid(*make(AV_PIX_FMT_YUV420P)));
    CHECK(typeid(*make(AV_PIX_FMT_YUVJ422P)) == typeid(*make(AV_PIX_FMT_YUV422P)));
    CHECK(typeid(*make(AV_PIX_FMT_YUVJ444P)) == typeid(*make(AV_PIX_FMT_YUV444P)));
    CHECK(typeid(*make(AV_PIX_FMT_YUVJ440P)) == typeid(*make(AV_PIX_FMT_YUV440P)));
  }
}

TEST_CASE("Semi-planar decoder selection", "[gfx][video][decoderfactory]")
{
  // NV12 and NV21 differ only in chroma plane order, which the decoder takes as
  // a constructor flag rather than as a separate class.
  const auto nv12 = make(AV_PIX_FMT_NV12);
  const auto nv21 = make(AV_PIX_FMT_NV21);
  REQUIRE(as<NV12Decoder>(nv12));
  REQUIRE(as<NV12Decoder>(nv21));

  CHECK(as<NV16Decoder>(make(AV_PIX_FMT_NV16)));
  CHECK(as<P010Decoder>(make(AV_PIX_FMT_P010LE)));
  CHECK(as<P016Decoder>(make(AV_PIX_FMT_P016LE)));

  CHECK(as<UYVY422Decoder>(make(AV_PIX_FMT_UYVY422)));
  CHECK(as<YUYV422Decoder>(make(AV_PIX_FMT_YUYV422)));
  // UYVY and YUYV are byte-swapped twins and must not collapse onto one class.
  CHECK_FALSE(as<UYVY422Decoder>(make(AV_PIX_FMT_YUYV422)));

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
  CHECK(as<NV24Decoder>(make(AV_PIX_FMT_NV24)));
  CHECK(as<NV24Decoder>(make(AV_PIX_FMT_NV42)));
#endif
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 17, 100)
  CHECK(as<P210Decoder>(make(AV_PIX_FMT_P210LE)));
  CHECK(as<P410Decoder>(make(AV_PIX_FMT_P410LE)));
#endif
}

TEST_CASE("Packed RGB decoder parameterisation", "[gfx][video][decoderfactory]")
{
  // These all share PackedDecoder, so the class is not the decision -- the
  // texture format, the stride and the swizzle are.
  struct Row
  {
    AVPixelFormat fmt;
    QRhiTexture::Format tex;
    int bytesPerPixel;
    const char* filterContains;
  };

  const Row rows[] = {
      {AV_PIX_FMT_RGB0, QRhiTexture::RGBA8, 4, "processed.a = 1.0;"},
      {AV_PIX_FMT_RGBA, QRhiTexture::RGBA8, 4, ""},
      {AV_PIX_FMT_BGR0, QRhiTexture::BGRA8, 4, "processed.a = 1.0;"},
      {AV_PIX_FMT_BGRA, QRhiTexture::BGRA8, 4, ""},
      {AV_PIX_FMT_ARGB, QRhiTexture::RGBA8, 4, "tex.yzwx"},
      {AV_PIX_FMT_ABGR, QRhiTexture::RGBA8, 4, "tex.abgr"},
      {AV_PIX_FMT_GRAY8, QRhiTexture::R8, 1, "vec4(tex.r, tex.r, tex.r, 1.0)"},
      {AV_PIX_FMT_GRAY16, QRhiTexture::R16, 2, "vec4(vec3(tex.r * 1.00390625), 1.0)"},
      {AV_PIX_FMT_GRAYF32, QRhiTexture::R32F, 4, "vec4(tex.r, tex.r, tex.r, 1.0)"},
      {AV_PIX_FMT_YA8, QRhiTexture::RG8, 2, "vec4(tex.r, tex.r, tex.r, tex.g)"},
      {AV_PIX_FMT_YA16LE, QRhiTexture::RG16, 4, "vec4(tex.rrr, tex.g) * 1.00390625"},
  };

  for(const auto& row : rows)
  {
    INFO("pixel format " << int(row.fmt));
    const auto d = make(row.fmt);
    auto* p = as<PackedDecoder>(d);
    REQUIRE(p != nullptr);
    CHECK(p->format == row.tex);
    CHECK(p->bytes_per_pixel == row.bytesPerPixel);
    if(*row.filterContains)
      CHECK(p->filter.contains(QLatin1String(row.filterContains)));
  }

  SECTION("the caller's filter is appended, not replaced")
  {
    const auto d = make(AV_PIX_FMT_RGB0, "processed.r *= 2.0;");
    auto* p = as<PackedDecoder>(d);
    REQUIRE(p != nullptr);
    CHECK(p->filter.contains(QLatin1String("processed.a = 1.0;")));
    CHECK(p->filter.contains(QLatin1String("processed.r *= 2.0;")));
  }

  SECTION("24- and 48-bit RGB have their own decoders")
  {
    // Three bytes per pixel is not expressible as a QRhi texture format.
    CHECK(as<RGB24Decoder>(make(AV_PIX_FMT_RGB24)));
    CHECK(as<RGB24Decoder>(make(AV_PIX_FMT_BGR24)));
    CHECK(as<RGB48Decoder>(make(AV_PIX_FMT_RGB48LE)));
    CHECK(as<RGB48Decoder>(make(AV_PIX_FMT_BGR48LE)));

    CHECK(as<RGBA64Decoder>(make(AV_PIX_FMT_RGBA64LE)));
    CHECK(as<RGBA64Decoder>(make(AV_PIX_FMT_BGRA64LE)));
  }
}

TEST_CASE("Planar RGB decoder parameterisation", "[gfx][video][decoderfactory]")
{
  {
    const auto d = make(AV_PIX_FMT_GBRP);
    auto* p = as<PlanarDecoder>(d);
    REQUIRE(p != nullptr);
    CHECK(p->planes == QLatin1String("gbr"));
    CHECK(p->format == QRhiTexture::R8);
    CHECK(p->bytes_per_pixel == 1);
  }
  {
    const auto d = make(AV_PIX_FMT_GBRAP);
    auto* p = as<PlanarDecoder>(d);
    REQUIRE(p != nullptr);
    CHECK(p->planes == QLatin1String("gbra"));
  }

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(56, 19, 100)
  SECTION("the high-bit-depth planes carry their own scaling factor")
  {
    // The 8-bit-equivalent code of an n-bit sample is code / 2^(n-8), so full
    // scale is 255 * 2^(n-8): 10-bit LSB-aligned data needs 65535/1020 = 64.25,
    // 12-bit needs 65535/4080 = 16.0625, and a 16-bit plane still needs
    // 65535/65280 = 1.00390625 -- not 1. Getting these wrong is a brightness
    // bug, not a crash.
    const auto d_p10 = make(AV_PIX_FMT_GBRP10LE);
    auto* p10 = as<PlanarDecoder>(d_p10);
    REQUIRE(p10 != nullptr);
    CHECK(p10->format == QRhiTexture::R16);
    CHECK(p10->bytes_per_pixel == 2);
    CHECK(p10->filter.contains(QLatin1String("*= 64.25")));

    const auto d_p12 = make(AV_PIX_FMT_GBRP12LE);
    auto* p12 = as<PlanarDecoder>(d_p12);
    REQUIRE(p12 != nullptr);
    CHECK(p12->filter.contains(QLatin1String("*= 16.0625")));

    const auto d_p16 = make(AV_PIX_FMT_GBRP16LE);
    auto* p16 = as<PlanarDecoder>(d_p16);
    REQUIRE(p16 != nullptr);
    CHECK(p16->filter.contains(QLatin1String("*= 1.00390625")));

    const auto d_pf32 = make(AV_PIX_FMT_GBRPF32LE);
    auto* pf32 = as<PlanarDecoder>(d_pf32);
    REQUIRE(pf32 != nullptr);
    CHECK(pf32->format == QRhiTexture::R32F);
    CHECK(pf32->bytes_per_pixel == 4);

    const auto d_pa32 = make(AV_PIX_FMT_GBRAPF32LE);
    auto* pa32 = as<PlanarDecoder>(d_pa32);
    REQUIRE(pa32 != nullptr);
    CHECK(pa32->planes == QLatin1String("gbra"));
  }
#endif
}

#if !defined(__EMSCRIPTEN__)
TEST_CASE("Fourcc-tagged codecs", "[gfx][video][decoderfactory]")
{
  // The default branch reads the four bytes of pixel_format as a codec tag.
  // HAPDecoder / DXVDecoder have out-of-line constructors, so their typeinfo
  // stays inside the hidden-visibility plugin and dynamic_cast from here would
  // need a symbol the test cannot see. typeid(*d) reads the vptr instead.
  const char* hapTags[] = {"Hap1", "Hap5", "HapY", "HapA", "Hap7", "HapH", "HapM"};
  for(const char* tag : hapTags)
  {
    INFO(tag);
    char t[5]{};
    std::memcpy(t, tag, 4);
    CHECK(makeFourcc(t) != nullptr);
  }

  const auto hap1 = makeFourcc("Hap1");
  const auto hapM = makeFourcc("HapM");
  REQUIRE(hap1);
  REQUIRE(hapM);
  // HapM is the only one with its own class; the rest differ by texture format.
  CHECK(typeid(*hap1) != typeid(*hapM));
  CHECK(typeid(*makeFourcc("Hap5")) == typeid(*hap1));

  const auto dxv1 = makeFourcc("Dxv1");
  const auto dxvY = makeFourcc("DxvY");
  REQUIRE(dxv1);
  REQUIRE(dxvY);
  CHECK(typeid(*dxv1) != typeid(*hap1));
  CHECK(typeid(*dxvY) != typeid(*dxv1));
  CHECK(typeid(*makeFourcc("Dxv5")) == typeid(*dxv1));
  CHECK(typeid(*makeFourcc("DxvA")) == typeid(*dxvY));

  SECTION("an unknown fourcc is not a decoder")
  {
    CHECK(makeFourcc("Zzzz") == nullptr);
    CHECK(makeFourcc("Hap9") == nullptr);
  }
}
#endif

TEST_CASE("Unsupported formats yield no decoder", "[gfx][video][decoderfactory]")
{
  // The caller is expected to fall back rather than to be handed an
  // EmptyDecoder that silently renders nothing.
  CHECK(make(AV_PIX_FMT_NONE) == nullptr);
  CHECK(make(AVPixelFormat(-12345)) == nullptr);
}

TEST_CASE("PixelFormatInfo from an AVPixelFormat", "[gfx][video][decoderfactory]")
{
  SECTION("chroma subsampling")
  {
    const auto p420 = PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV420P);
    CHECK(p420.log2ChromaW == 1);
    CHECK(p420.log2ChromaH == 1);

    const auto p422 = PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV422P);
    CHECK(p422.log2ChromaW == 1);
    CHECK(p422.log2ChromaH == 0);

    const auto p444 = PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV444P);
    CHECK(p444.log2ChromaW == 0);
    CHECK(p444.log2ChromaH == 0);
  }

  SECTION("bit depth and the 8-bit predicate")
  {
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV420P).bitDepth == 8);
    CHECK_FALSE(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV420P).is10bit());
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV420P10LE).bitDepth == 10);
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV420P10LE).is10bit());
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV420P12LE).bitDepth == 12);
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_P016LE).bitDepth == 16);
  }

  SECTION("plane count and alpha")
  {
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV420P).numPlanes == 3);
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_NV12).numPlanes == 2);
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_RGBA).numPlanes == 1);

    CHECK_FALSE(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUV420P).hasAlpha);
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_YUVA420P).hasAlpha);
    CHECK(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_RGBA).hasAlpha);
    CHECK_FALSE(PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_RGB24).hasAlpha);
  }

  SECTION("an unknown format leaves the defaults in place")
  {
    const auto info = PixelFormatInfo::fromAVPixelFormat(AV_PIX_FMT_NONE);
    CHECK(info.log2ChromaW == 1);
    CHECK(info.log2ChromaH == 1);
    CHECK(info.bitDepth == 8);
    CHECK(info.numPlanes == 2);
    CHECK_FALSE(info.hasAlpha);
  }
}

TEST_CASE("PixelFormatInfo from codec parameters", "[gfx][video][decoderfactory]")
{
  SECTION("the hardware sw_format wins over the codecpar format")
  {
    const auto info = PixelFormatInfo::fromCodecParameters(
        AV_PIX_FMT_P010LE, AV_PIX_FMT_YUV420P, 0);
    CHECK(info.bitDepth == 10);
    CHECK(info.numPlanes == 2);
  }

  SECTION("without a sw_format the codecpar format is used")
  {
    const auto info = PixelFormatInfo::fromCodecParameters(
        AV_PIX_FMT_NONE, AV_PIX_FMT_YUV444P, 0);
    CHECK(info.log2ChromaW == 0);
    CHECK(info.numPlanes == 3);
  }

  SECTION("bits_per_raw_sample only ever raises the depth")
  {
    // Some containers report a deeper raw sample than the pixel format admits;
    // a lower one is noise and must not shrink it.
    CHECK(
        PixelFormatInfo::fromCodecParameters(AV_PIX_FMT_NONE, AV_PIX_FMT_YUV420P, 10)
            .bitDepth
        == 10);
    CHECK(
        PixelFormatInfo::fromCodecParameters(
            AV_PIX_FMT_NONE, AV_PIX_FMT_YUV420P10LE, 8)
            .bitDepth
        == 10);
    CHECK(
        PixelFormatInfo::fromCodecParameters(AV_PIX_FMT_NONE, AV_PIX_FMT_YUV420P, 0)
            .bitDepth
        == 8);
  }

  SECTION("neither format known leaves the defaults")
  {
    const auto info
        = PixelFormatInfo::fromCodecParameters(AV_PIX_FMT_NONE, AV_PIX_FMT_NONE, 0);
    CHECK(info.bitDepth == 8);
    CHECK(info.numPlanes == 2);
  }
}
